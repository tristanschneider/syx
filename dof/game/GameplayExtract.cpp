#include "Precompile.h"
#include "GameplayExtract.h"

#include "CommonTasks.h"
#include "Simulation.h"
#include "TableAdapters.h"

namespace GameplayExtract {
  using namespace Tags;

  template<class TableT>
  void extractTable(GameDB db, TaskNode& root) {
    auto& table = std::get<TableT>(db.db.mTables);
    //TODO: this would be the same almost always for the static table
    root.mChildren.push_back(CommonTasks::moveOrCopyRowSameSize<FloatRow<Pos, X>, FloatRow<GPos, X>>(table));
    root.mChildren.push_back(CommonTasks::moveOrCopyRowSameSize<FloatRow<Pos, Y>, FloatRow<GPos, Y>>(table));
    root.mChildren.push_back(CommonTasks::moveOrCopyRowSameSize<FloatRow<Rot, CosAngle>, FloatRow<GRot, CosAngle>>(table));
    root.mChildren.push_back(CommonTasks::moveOrCopyRowSameSize<FloatRow<Rot, SinAngle>, FloatRow<GRot, SinAngle>>(table));
    //If it has x velocity it should have the others
    if constexpr(TableOperations::hasRow<FloatRow<LinVel, X>, TableT>()) {
      root.mChildren.push_back(CommonTasks::moveOrCopyRowSameSize<FloatRow<LinVel, X>, FloatRow<GLinVel, X>>(table));
      root.mChildren.push_back(CommonTasks::moveOrCopyRowSameSize<FloatRow<LinVel, Y>, FloatRow<GLinVel, Y>>(table));
      root.mChildren.push_back(CommonTasks::moveOrCopyRowSameSize<FloatRow<AngVel, Angle>, FloatRow<GAngVel, Angle>>(table));
    }
  }

  TaskRange extractGameplayData(GameDB db) {
    auto root = TaskNode::createEmpty();
    extractTable<GameObjectTable>(db, *root);
    extractTable<StaticGameObjectTable>(db, *root);
    extractTable<PlayerTable>(db, *root);
    return TaskBuilder::addEndSync(root);
  }
}