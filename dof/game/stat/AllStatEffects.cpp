#include "Precompile.h"
#include "stat/AllStatEffects.h"

#include "CommonTasks.h"
#include "curve/CurveSolver.h"
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
    CommonTasks::Now::moveOrCopyRow(src, dst, dstStart);
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

  template<class T>
  constexpr void getCurveTag(const T&);
  template<class T>
  constexpr auto getCurveTag(const CurveInput<T>&) -> T;

  std::shared_ptr<TaskNode> solveCurves(Row<float>& curveInput, Row<float>& curveOutput, Row<CurveDefinition*>& definition, const float* dt) {
    auto root = TaskNode::create([&, dt](...) {
      for(size_t i = 0; i < curveInput.size(); ++i) {
        //Another possibility would be scanning forward to look for matching definitions so they can be solved in groups
        CurveSolver::CurveUniforms uniforms{ 1 };
        //Update input time in place
        CurveSolver::CurveVaryings varyings{ &curveInput.at(i), &curveInput.at(i) };
        CurveSolver::advanceTime(*definition.at(i), uniforms, varyings, *dt);
      }
    });
    root->mChildren.push_back(TaskNode::create([&](...) {
      for(size_t i = 0; i < curveInput.size(); ++i) {
        CurveSolver::CurveUniforms uniforms{ 1 };
        CurveSolver::CurveVaryings varyings{ &curveInput.at(i), &curveOutput.at(i) };
        CurveSolver::solve(*definition.at(i), uniforms, varyings);
      }
    }));
    return root;
  }

  template<class TableT>
  void visitSolveCurves(TableT& table, const float* dt, [[maybe_unused]] std::vector<std::shared_ptr<TaskNode>>& results) {
    table.visitOne([&](auto& row) {
      using Tag = decltype(getCurveTag(row));
      if constexpr(!std::is_same_v<void, Tag>) {
        results.push_back(solveCurves(
          TableOperations::getRow<CurveInput<Tag>>(table),
          TableOperations::getRow<CurveOutput<Tag>>(table),
          TableOperations::getRow<CurveDef<Tag>>(table),
          dt
        ));
      }
    });
  }

  template<class TableT>
  void visitResolveOwners(TableT& table, GameDB db, std::vector<std::shared_ptr<TaskNode>>& results) {
    //Stable info for the owners that the stats are pointing to
    StableInfo stable;
    stable.description = GameDatabase::getDescription();
    stable.mappings = &TableAdapters::getStableMappings(db);

    if(StatEffect::Target* target = TableOperations::tryGetRow<StatEffect::Target>(table)) {
      results.push_back(StatEffect::resolveOwners(db, *target, stable));
    }
    results.push_back(StatEffect::resolveOwners(db, std::get<StatEffect::Owner>(table.mRows), stable));
  }

  AllStatTasks createTasks(GameDB db, StatEffectDatabase& stats) {
    AllStatTasks result;
    result.positionSetters = StatEffect::processStat(std::get<PositionStatEffectTable>(stats.mTables), db)
      .then(StatEffect::processStat(std::get<FollowTargetByPositionStatEffectTable>(stats.mTables), db));
    result.velocitySetters = StatEffect::processStat(std::get<VelocityStatEffectTable>(stats.mTables), db);
    result.posGetVelSet = StatEffect::processStat(std::get<AreaForceStatEffectTable>(stats.mTables), db);
    TaskRange lambdaRange = StatEffect::processStat(std::get<LambdaStatEffectTable>(stats.mTables), db);
    const float* dt = &TableAdapters::getConfig(db).game->world.deltaTime;
    //Empty root
    auto syncBegin = TaskNode::create([](...){});
    //Then process all lifetimes
    auto& globals = getGlobals(stats);
    visitStats(stats, [syncBegin, dt](auto& table) {
      syncBegin->mChildren.push_back(visitTickLifetime(table));
      //Curves only depend on themselves so can be solved here
      visitSolveCurves(table, dt, syncBegin->mChildren);
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
      visitResolveOwners(table, db, currentSync->mChildren);
    });

    auto syncEnd = TaskNode::create([](...){});
    TaskBuilder::_addSyncDependency(*currentSync, syncEnd);

    //Once the synchronous tasks are complete, start parallel tasks
    syncEnd->mChildren.push_back(result.positionSetters.mBegin);
    syncEnd->mChildren.push_back(result.velocitySetters.mBegin);

    //Position and velocity need to be done before both can be used together
    result.positionSetters.mEnd->mChildren.push_back(result.posGetVelSet.mBegin);
    result.velocitySetters.mEnd->mChildren.push_back(result.posGetVelSet.mBegin);

    result.synchronous = TaskBuilder::addEndSync(syncBegin);
    return result;
  }
}