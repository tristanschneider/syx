#include "Precompile.h"
#include "stat/AllStatEffects.h"

#include "CommonTasks.h"
#include "curve/CurveSolver.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"

StatEffectDatabase& StatEffectDatabase::get(AppTaskArgs& task) {
  return *ThreadLocalData::get(task).statEffects;
}

StableElementMappings& StatEffectDatabase::getMappings(AppTaskArgs& task) {
  return *ThreadLocalData::get(task).mappings;
}

StatEffectDatabase::StatEffectDatabase() {
  //Assign unique ids to each stat table. Doesn't matter what they are as long as this behaves the same
  //across the central and thread local instances of this
  size_t curTable = 0;
  visitOne([&curTable](auto& table) {
    if(StatEffect::Global* global = TableOperations::tryGetRow<StatEffect::Global>(table)) {
      global->at().ID = curTable++;
    }
  });
}

namespace StatEffect {
  template<class Func>
  void visitStats(StatEffectDatabase& stats, const Func& func) {
    stats.visitOne([&func](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr(TableOperations::hasRow<StatEffect::Global, TableT>()) {
        func(table);
      }
    });
  }

  template<class T>
  void visitMoveToRow(const Row<T>& src, Row<T>& dst, size_t dstStart) {
    CommonTasks::Now::moveOrCopyRow(src, dst, dstStart);
  }

  void visitMoveToRow(const StableIDRow&, StableIDRow&, size_t) {
    //Stable row is left unchanged as the resize itself creates the necessary ids
  }

  template<class T>
  void visitMoveToRow(const SharedRow<T>&, SharedRow<T>&, size_t) {
    //Nothing to do for shared rows
  }

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
        StatEffectDatabase& threadDB = *tls.get(i).statEffects;
        visitStats(threadDB, [&](auto& fromTable) {
          const size_t srcSize = TableOperations::size(fromTable);
          if(!srcSize) {
            return;
          }

          using TableT = std::decay_t<decltype(fromTable)>;
          //The index in the destination where the src elements should begin
          size_t newBegin{};
          //This is a hack assuming the tables are the same
          //Ideally both would use RuntimeDatabase and could generically copy all rows
          const size_t tableAssociation = std::get<StatEffect::Global>(fromTable.mRows).at().ID;
          assert(tableAssociation < centralStatTables.size());
          const TableID toTableID = centralStatTables.matchingTableIDs[tableAssociation];
          RuntimeTable* runtimeToTable = db.tryGet(toTableID);
          assert(runtimeToTable);
          TableT& toTable = *static_cast<TableT*>(runtimeToTable->stableModifier.table);

          const size_t dstSize = TableOperations::size(toTable);
          newBegin = dstSize;

          runtimeToTable->stableModifier.resize(dstSize + srcSize, nullptr);

          //Publish any newly added ids below
          StatEffect::Global& g = std::get<StatEffect::Global>(toTable.mRows);

          //Resize the table, then fill in each row
          toTable.visitOne([&](auto& toRow) {
            using RowT = std::decay_t<decltype(toRow)>;
            RowT& fromRow = std::get<RowT>(fromTable.mRows);
            visitMoveToRow(fromRow, toRow, newBegin);
          });

          //Notify of all the newly created stable ids from the resize above
          StableIDRow& stableRow = std::get<StableIDRow>(toTable.mRows);
          g.at().newlyAdded.insert(g.at().newlyAdded.end(), stableRow.begin() + newBegin, stableRow.end());

          //Once all rows in the destination have the desired values, clear the source
          StableElementMappings& srcMappings = db.getMappings();
          StableOperations::stableResizeTable(fromTable, TableID{ UnpackedDatabaseElementID::fromPacked(StatEffectDatabase::getTableIndex(fromTable)) }, 0, srcMappings);
        });
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
}