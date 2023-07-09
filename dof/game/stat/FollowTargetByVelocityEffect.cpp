#include "Precompile.h"
#include "stat/FollowTargetByVelocityEffect.h"

#include "glm/glm.hpp"
#include "Simulation.h"
#include "TableAdapters.h"
#include "GameMath.h"

namespace StatEffect {
  struct VisitArgs {
    glm::vec2 srcPos{};
    glm::vec2 dstPos{};
  };
  Math::Impulse visitComputeImpulse(const FollowTargetByVelocityStatEffect::SpringFollow& follow, const VisitArgs& args) {
    return {
      (args.dstPos - args.srcPos)*follow.springConstant,
      0.0f
    };
  }

  TaskRange processStat(FollowTargetByVelocityStatEffectTable& table, GameDB db) {
    auto task = TaskNode::create([&table, db](...) {
      auto follow = TableAdapters::getCentralStatEffects(db).followTargetByVelocity;
      for(size_t i = 0; i < follow.command->size(); ++i) {
        const auto self = follow.base.owner->at(i).toPacked<GameDatabase>();
        const auto target = follow.base.target->at(i).toPacked<GameDatabase>();
        if(!self.isValid() || !target.isValid()) {
          continue;
        }
        const FollowTargetByVelocityStatEffect::Command& cmd = follow.command->at(i);
        const size_t selfI = self.getElementIndex();
        const size_t targetI = target.getElementIndex();

        auto impulseX = Queries::getRowInTable<FloatRow<Tags::GLinImpulse, Tags::X>>(db.db, self);
        auto impulseY = Queries::getRowInTable<FloatRow<Tags::GLinImpulse, Tags::Y>>(db.db, self);
        if(impulseX && impulseY) {
          auto srcPosX = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::X>>(db.db, self);
          auto srcPosY = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::Y>>(db.db, self);
          auto dstPosX = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::X>>(db.db, target);
          auto dstPosY = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::Y>>(db.db, target);

          VisitArgs args {
            TableAdapters::read(selfI, *srcPosX, *srcPosY),
            TableAdapters::read(targetI, *dstPosX, *dstPosY)
          };
          const Math::Impulse impulse = std::visit([&](const auto& c) { return visitComputeImpulse(c, args); }, cmd.mode);

          TableAdapters::add(selfI, impulse.linear, *impulseX, *impulseY);
        }
      }
    });
    return TaskBuilder::addEndSync(task);
  }
}