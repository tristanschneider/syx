#include "Precompile.h"
#include "stat/FollowTargetByPositionEffect.h"

#include "glm/glm.hpp"
#include "Simulation.h"
#include "TableAdapters.h"

namespace StatEffect {
  TaskRange processStat(FollowTargetByPositionStatEffectTable& table, GameDB db) {
    auto task = TaskNode::create([&table, db](...) {
      auto follow = TableAdapters::getCentralStatEffects(db).followTargetByPosition;
      for(size_t i = 0; i < follow.command->size(); ++i) {
        const auto self = follow.base.owner->at(i).toPacked<GameDatabase>();
        const auto target = follow.base.target->at(i).toPacked<GameDatabase>();
        if(!self.isValid() || !target.isValid()) {
          continue;
        }

        const FollowTargetByPositionStatEffect::Command& cmd = follow.command->at(i);
        auto srcPosX = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::X>>(db.db, self);
        auto srcPosY = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::Y>>(db.db, self);
        auto dstPosX = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::X>>(db.db, target);
        auto dstPosY = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::Y>>(db.db, target);
        const size_t selfI = self.getElementIndex();
        const size_t targetI = target.getElementIndex();

        glm::vec2 src{ srcPosX->at(selfI), srcPosY->at(selfI) };
        glm::vec2 dst{ dstPosX->at(targetI), dstPosY->at(targetI) };
        const float curveOutput = follow.base.curveOutput->at(i);
        const glm::vec2 toDst = dst - src;
        const float dist2 = glm::dot(toDst, toDst);

        if(dist2 > 0.001f) {
          switch(cmd.mode) {
            case FollowTargetByPositionStatEffect::FollowMode::Interpolation:
              src = glm::mix(src, dst, curveOutput);
              break;

            case FollowTargetByPositionStatEffect::FollowMode::Movement: {
              //Advance cuveOutput amount towards dst without overshooting it
              src = toDst * std::min(1.0f, (curveOutput/std::sqrt(dist2)));
              break;
            }
          }

          srcPosX->at(selfI) = src.x;
          srcPosY->at(selfI) = src.y;
        }
        else {
          //Reset curve input when reaching destination
          follow.base.curveInput->at(i) = 0.0f;
        }
      }
    });
    return TaskBuilder::addEndSync(task);
  }
}