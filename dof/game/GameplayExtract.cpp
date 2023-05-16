#include "Precompile.h"
#include "GameplayExtract.h"

#include "CommonTasks.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "stat/AllStatEffects.h"

namespace GameplayExtract {
  using namespace Tags;

  //TODO: would be more flexible to use Queries::viewEachRow to get all of these
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

  //Read/Write GLinImpulse, GAngImpulse
  //Read StableIDRow
  //Modify thread local
  TaskRange applyGameplayImpulses(GameDB db) {
    auto root = TaskNode::createEmpty();
    Queries::viewEachRow(db.db, [&root, db](
        FloatRow<GLinImpulse, X>& x,
        FloatRow<GLinImpulse, Y>& y,
        FloatRow<GAngImpulse, Angle>& a,
        StableIDRow& stable) {

      root->mChildren.push_back(TaskNode::create([&, db](enki::TaskSetPartition, uint32_t thread) {
        VelocityStatEffectAdapter v = TableAdapters::getVelocityEffects(db, thread);
        for(size_t i = 0; i < x.size(); ++i) {
          if(x.at(i) != 0 || y.at(i) != 0 || a.at(i) != 0) {
            const size_t id = TableAdapters::addStatEffectsSharedLifetime(v.base, StatEffect::INSTANT, &stable.at(i), 1);
            v.command->at(id) = VelocityStatEffect::VelocityCommand{
              glm::vec2{ x.at(i), y.at(i) },
              a.at(i)
            };
          }
          x.at(i) = y.at(i) = a.at(i) = 0;
        }
      }));
    });

    return TaskBuilder::addEndSync(root);
  }
}