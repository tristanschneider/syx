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

    task.setCallback([&db, &tls, centralStatTables](AppTaskArgs&) mutable {
      for(size_t i = 0; i < tls.getThreadCount(); ++i) {
        size_t curTable = 0;
        StatEffectDatabase& threadDB = tls.get(i).statEffects->db;
        visitStats(threadDB, [&](auto& fromTable) {
          using TableT = std::decay_t<decltype(fromTable)>;
          //The index in the destination where the src elements should begin
          size_t newBegin{};
          //This is a hack that assumes the iteration order of the db.query will match the visitStats order
          //Ideally both would use RuntimeDatabase and could generically copy all rows
          const UnpackedDatabaseElementID toTableID = centralStatTables.matchingTableIDs[curTable];
          RuntimeTable* runtimeToTable = db.tryGet(toTableID);
          assert(runtimeToTable);
          TableT& toTable = *static_cast<TableT*>(runtimeToTable->stableModifier.table);

          const size_t srcSize = TableOperations::size(fromTable);
          const size_t dstSize = TableOperations::size(toTable);
          newBegin = dstSize;

          if(!srcSize) {
            return;
          }

          runtimeToTable->stableModifier.resize(dstSize + srcSize, nullptr);

          //Resize the table, then fill in each row
          toTable.visitOne([&](auto& toRow) {
            using RowT = std::decay_t<decltype(toRow)>;
            RowT& fromRow = std::get<RowT>(fromTable.mRows);
            visitMoveToRow(fromRow, toRow, newBegin);
          });

          //Once all rows in the destination have the desired values, clear the source
          StableElementMappings& srcMappings = getGlobals(threadDB).stableMappings;
          TableOperations::stableResizeTable(fromTable, UnpackedDatabaseElementID::fromPacked(StatEffectDatabase::getTableIndex(fromTable)), 0, srcMappings);

          ++curTable;
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
    VelocityStatEffect::processStat(builder);
    AreaForceStatEffect::processStat(builder);
    FollowTargetByVelocityStatEffect::processStat(builder);
    LambdaStatEffect::processStat(builder);
  }
}