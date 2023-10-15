#include "Precompile.h"
#include "stat/AllStatEffects.h"

#include "CommonTasks.h"
#include "curve/CurveSolver.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
namespace StatEffect {
  template<class Func>
  void visitStats(StatEffectDatabase& stats, const Func& func) {
    stats.visitOne([&func](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr(std::is_same_v<AllStatEffects::GlobalTable, TableT>) {
      }
      else {
        func(table);
      }
    });
  }

  AllStatEffects::Globals& getGlobals(StatEffectDatabase& db) {
    return std::get<AllStatEffects::GlobalRow>(std::get<AllStatEffects::GlobalTable>(db.mTables).mRows).at();
  }

  void initGlobals(StatEffectDatabase& db) {
    AllStatEffects::Globals& globals = getGlobals(db);
    globals.description = StatEffectDatabase::getDescription();
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

  /* TODO:
  void moveThreadLocalToCentral(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("move stats");
    ThreadLocals& tls = TableAdapters::getThreadLocals(task);
    for(size_t i = 0; i < tls.getThreadCount(); ++i) {
      StatEffectDBOwned* threadDB = tls.get(i).statEffects;
      std::vector<std::function<void()>> work;
      visitStats(threadDB->db, [&](auto& table) {

      });
      //tls.get(0).statEffects
    }
  }

  TaskRange moveTo(StatEffectDatabase& from, StatEffectDatabase& to) {
    auto root = TaskNode::create([](...){});
    visitStats(to, [&](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      //The index in the destination where the src elements should begin
      auto newBegin = std::make_shared<size_t>();
      TableT& fromTable = std::get<TableT>(from.mTables);
      TableT& toTable = table;

      auto resizeDest = TaskNode::create([&fromTable, &toTable, &to, newBegin](...) {
        const size_t srcSize = TableOperations::size(fromTable);
        const size_t dstSize = TableOperations::size(toTable);
        *newBegin = dstSize;
        StableElementMappings& dstMappings = getGlobals(to).stableMappings;
        TableOperations::stableResizeTable(toTable, UnpackedDatabaseElementID::fromPacked(StatEffectDatabase::getTableIndex(toTable)), dstSize + srcSize, dstMappings);
      });
      root->mChildren.push_back(resizeDest);

      //Resize the table, then fill in each row
      table.visitOne([&, newBegin](auto& toRow) {
        using RowT = std::decay_t<decltype(toRow)>;
        RowT& fromRow = std::get<RowT>(fromTable.mRows);
        resizeDest->mChildren.push_back(TaskNode::create([&fromRow, &toRow, newBegin](...) {
          visitMoveToRow(fromRow, toRow, *newBegin);
        }));
      });

      //Once all rows in the destination have the desired values, clear the source
      auto resizeSrc = TaskNode::create([&fromTable, &from](...) {
        StableElementMappings& srcMappings = getGlobals(from).stableMappings;
        TableOperations::stableResizeTable(fromTable, UnpackedDatabaseElementID::fromPacked(StatEffectDatabase::getTableIndex(fromTable)), 0, srcMappings);
      });
      TaskBuilder::_addSyncDependency(*resizeDest, resizeSrc);
    });
    return TaskBuilder::addEndSync(root);
  }
  */
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
    for(ConfigRow* config : temp.query<ConfigRow, LambdaStatEffect::LambdaRow>().get<0>()) {
      //Hack to account for lambda processing being the only table that is scheduled before the lifetime update,
      //so it needs to be removed one tick earlier so that it's only called once instead of twice
      config->at().removeLifetime = 1;
    }

    for(ConfigRow* config : temp.query<ConfigRow, FollowTargetByPositionStatEffect::CommandRow>().get<0>()) {
      //Hack to account for lambda processing being the only table that is scheduled before the lifetime update,
      //so it needs to be removed one tick earlier so that it's only called once instead of twice
      config->at().curves.push_back(getCurveAlias<>());
    }

    temp.discard();
  }

  void createTasks(IAppBuilder& builder) {
    configureTables(builder);

    auto temp = builder.createTask();
    temp.discard();
    for(auto table : builder.queryTables<StatEffect::Global>().matchingTableIDs) {
      const Config config = *temp.query<StatEffect::ConfigRow>(table).tryGetSingletonElement();
      StatEffect::tickLifetime(&builder, table, config.removeLifetime);
      StatEffect::processCompletionContinuation(builder, table);
      for(const CurveAlias& curve : config.curves) {
        StatEffect::solveCurves(builder, table, curve);
      }
      StatEffect::processRemovals(builder, table);
      StatEffect::resolveOwners(builder, table);
      StatEffect::resolveTargets(builder, table);
    }

    PositionStatEffect::processStat(builder);
    FollowTargetByPositionStatEffect::processStat(builder);
    DamageStatEffect::processStat(builder);
    VelocityStatEffect::processState(builder);
    AreaForceStatEffect::processStat(builder);
    FollowTargetByVelocityStatEffect::processStat(builder);
    LambdaStatEffect::processStat(builder);
  }
}