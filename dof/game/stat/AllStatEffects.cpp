#include "Precompile.h"
#include "stat/AllStatEffects.h"

#include "stat/AreaForceStatEffect.h"
#include "stat/ConstraintStatEffect.h"
#include "stat/DamageStatEffect.h"
#include "stat/FollowTargetByPositionEffect.h"
#include "stat/FollowTargetByVelocityEffect.h"
#include "stat/FragmentBurstStatEffect.h"
#include "stat/LambdaStatEffect.h"
#include "stat/PositionStatEffect.h"
#include "stat/VelocityStatEffect.h"
#include "CommonTasks.h"
#include "curve/CurveSolver.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"

RuntimeDatabase& StatEffectDatabase::get(AppTaskArgs& task) {
  return *ThreadLocalData::get(task).statEffects;
}

StableElementMappings& StatEffectDatabase::getMappings(AppTaskArgs& task) {
  return *ThreadLocalData::get(task).mappings;
}

namespace StatEffect {
  void moveThreadLocalToCentral(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("move stats");
    RuntimeDatabase& db = task.getDatabase();
    ThreadLocals& tls = TableAdapters::getThreadLocals(task);
    auto centralStatTables = db.query<StatEffect::Global>();
    for(size_t i = 0; i < centralStatTables.size(); ++i) {
      assert(centralStatTables.get<0>(i).at().ID == i && "This assumes ascending order, if it's not, a mapping is needed here");
    }

    task.setCallback([&db, &tls, centralStatTables](AppTaskArgs&) mutable {
      for(size_t i = 0; i < tls.getThreadCount(); ++i) {
        RuntimeDatabase& threadDB = *tls.get(i).statEffects;
        auto query = threadDB.query<StatEffect::Global>();
        for(size_t q = 0; q < query.size(); ++q) {
          RuntimeTable* fromTable = threadDB.tryGet(query.matchingTableIDs[q]);
          assert(fromTable);

          const size_t srcSize = fromTable->size();
          if(!srcSize) {
            return;
          }

          //This is a hack assuming the tables are the same
          //Ideally both would use RuntimeDatabase and could generically copy all rows
          const size_t tableAssociation = query.get<0>(q).at().ID;
          assert(tableAssociation < centralStatTables.size());
          const TableID toTableID = centralStatTables.matchingTableIDs[tableAssociation];
          RuntimeTable* runtimeToTable = db.tryGet(toTableID);
          assert(runtimeToTable);

          //The index in the destination where the src elements should begin
          const size_t newBegin = RuntimeTable::migrate(0, *fromTable, *runtimeToTable, srcSize);

          //Publish any newly added ids below
          StatEffect::Global& g = *runtimeToTable->tryGet<StatEffect::Global>();
          //Notify of all the newly created stable ids from the resize above
          StableIDRow& stableRow = *runtimeToTable->tryGet<StableIDRow>();
          //TODO: why not use DBEvents for this?
          g.at().newlyAdded.insert(g.at().newlyAdded.end(), stableRow.begin() + newBegin, stableRow.end());
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  template<class Tag = DefaultCurveTag>
  CurveAlias getCurveAlias() {
    return {
      QueryAlias<Row<float>>::create<CurveInput<Tag>>(),
      QueryAlias<Row<float>>::create<CurveOutput<Tag>>(),
      QueryAlias<Row<CurveDefinition*>>::create<CurveDef<Tag>>()
    };
  }

  void configureTables(IAppBuilder& builder) {
    auto temp = builder.createTask();
    temp.query<ConfigRow, FollowTargetByPositionStatEffect::CommandRow>().forEachRow([](ConfigRow& config, auto&&) {
      //Hack to account for lambda processing being the only table that is scheduled before the lifetime update,
      //so it needs to be removed one tick earlier so that it's only called once instead of twice
      config.at().curves.push_back(getCurveAlias<>());
    });

    temp.discard();
  }

  void init(IAppBuilder& builder) {
    ConstraintStatEffect::initStat(builder);
  }

  void createTasks(IAppBuilder& builder) {
    configureTables(builder);

    auto temp = builder.createTask();
    temp.discard();
    for(auto table : builder.queryTables<StatEffect::Global>().matchingTableIDs) {
      const Config config = *temp.query<StatEffect::ConfigRow>(table).tryGetSingletonElement();
      //Lifetime is ticked first meaning the stat will be in the toremove list for one visible invocation of processStat
      //Any stats that want to do something on removal can use the global toremove list on their table
      StatEffect::tickLifetime(&builder, table, config.removeLifetime);
      StatEffect::processCompletionContinuation(builder, table);
      for(const CurveAlias& curve : config.curves) {
        StatEffect::solveCurves(builder, table, curve);
      }
    }

    PositionStatEffect::processStat(builder);
    FollowTargetByPositionStatEffect::processStat(builder);
    DamageStatEffect::processStat(builder);
    VelocityStatEffect::processStat(builder);
    AreaForceStatEffect::processStat(builder);
    FollowTargetByVelocityStatEffect::processStat(builder);
    FragmentBurstStatEffect::processStat(builder);
    ConstraintStatEffect::processStat(builder);

    //Goes last since nothing can run in parallel with this.
    //TODO: get rid of the need for this with only specific tasks
    LambdaStatEffect::processStat(builder);

    //Remove stats at the end after all processStats have had a chance to make any final changes
    for(auto table : builder.queryTables<StatEffect::Global>().matchingTableIDs) {
      StatEffect::processRemovals(builder, table);
    }
  }

  struct StatStorage : ChainedRuntimeStorage {
    using DB = Database<
      LambdaStatEffectTable,
      PositionStatEffectTable,
      VelocityStatEffectTable,
      AreaForceStatEffectTable,
      FollowTargetByPositionStatEffectTable,
      FollowTargetByVelocityStatEffectTable,
      FragmentBurstStatEffectTable,
      DamageStatEffectTable,
      ConstraintStatEffectTable
    >;

    StatStorage(RuntimeDatabaseArgs& args)
      : ChainedRuntimeStorage(args)
    {
      //Assign unique ids to each stat table. Doesn't matter what they are as long as this behaves the same
      //across the central and thread local instances of this
      size_t curTable = 0;
      db.visitOne([&curTable](auto& table) {
        if(StatEffect::Global* global = &std::get<StatEffect::Global>(table.mRows)) {
          global->at().ID = curTable++;
        }
      });
    }

    DB db;
  };

  void createDatabase(RuntimeDatabaseArgs& args) {
    RuntimeStorage::addToChain<StatStorage>(args);
  }
}