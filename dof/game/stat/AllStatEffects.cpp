#include "Precompile.h"
#include "stat/AllStatEffects.h"

#include "Simulation.h"
#include "TableAdapters.h"

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
    if constexpr(std::is_trivially_copyable_v<T>) {
      if(size_t size = src.size()) {
        std::memcpy(&dst.at(dstStart), &src.at(0), sizeof(T)*size);
      }
    }
    else {
      for(size_t i = 0; i < src.size(); ++i) {
        dst.at(i) = std::move(src.at(i));
      }
    }
  }

  void visitMoveToRow(const StableIDRow&, StableIDRow&, size_t) {
    //Stable row is left unchanged as the resize itself creates the necessary ids
  }

  template<class T>
  void visitMoveToRow(const SharedRow<T>&, SharedRow<T>&, size_t) {
    //Nothing to do for shared rows
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

  template<class TableT>
  std::shared_ptr<TaskNode> visitTickLifetime(TableT& table) {
    return StatEffect::tickLifetime(
      std::get<StatEffect::Lifetime>(table.mRows),
      std::get<StableIDRow>(table.mRows),
      std::get<StatEffect::Global>(table.mRows).at().toRemove);
  }

  template<class TableT>
  std::shared_ptr<TaskNode> visitRemoveLifetime(TableT& table, AllStatEffects::Globals& globals) {
    //Stable info for the ids of the stats
    StableInfo stable;
    stable.description = globals.description;
    stable.mappings = &globals.stableMappings;
    stable.row = &std::get<StableIDRow>(table.mRows);
    return StatEffect::processRemovals(table, stable);
  }

  template<class TableT>
  std::shared_ptr<TaskNode> visitResolveOwners(TableT& table, GameDB db) {
    //Stable info for the owners that the stats are pointing to
    StableInfo stable;
    stable.description = GameDatabase::getDescription();
    stable.mappings = &TableAdapters::getStableMappings(db);
    return StatEffect::resolveOwners(db, std::get<StatEffect::Owner>(table.mRows), stable);
  }

  AllStatTasks createTasks(GameDB db, StatEffectDatabase& stats) {
    AllStatTasks result;
    result.positionSetters = StatEffect::processStat(std::get<PositionStatEffectTable>(stats.mTables), db);
    result.velocitySetters = StatEffect::processStat(std::get<VelocityStatEffectTable>(stats.mTables), db);
    TaskRange lambdaRange = StatEffect::processStat(std::get<LambdaStatEffectTable>(stats.mTables), db);
    //Empty root
    auto syncBegin = TaskNode::create([](...){});
    //Then process all lifetimes
    auto& globals = getGlobals(stats);
    visitStats(stats, [syncBegin](auto& table) {
      syncBegin->mChildren.push_back(visitTickLifetime(table));
    });

    TaskBuilder::_addSyncDependency(*syncBegin, lambdaRange.mBegin);
    auto currentSync = lambdaRange.mEnd;

    //After ticking lifetimes, synchronously do removal. Could be parallel if stable mappings were locked
    TaskBuilder::_addSyncDependency(*syncBegin, currentSync);
    visitStats(stats, [&currentSync, &globals](auto& table) {
      currentSync->mChildren.push_back(visitRemoveLifetime(table, globals));
      currentSync = currentSync->mChildren.back();
    });
    //Next, pre-resolve all handles
    visitStats(stats, [currentSync, db](auto& table) {
      currentSync->mChildren.push_back(visitResolveOwners(table, db));
    });

    auto syncEnd = TaskNode::create([](...){});
    TaskBuilder::_addSyncDependency(*currentSync, syncEnd);

    //Once the synchronous tasks are complete, start parallel tasks
    syncEnd->mChildren.push_back(result.positionSetters.mBegin);
    syncEnd->mChildren.push_back(result.velocitySetters.mBegin);

    result.synchronous = TaskBuilder::addEndSync(syncBegin);
    return result;
  }
}