#include "Precompile.h"
#include "stat/FollowTargetByPositionEffect.h"

#include "glm/glm.hpp"
#include "Simulation.h"
#include "TableAdapters.h"

#include "AppBuilder.h"

namespace FollowTargetByPositionStatEffect {
  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("FollowTargetByPosition Stat");
    using namespace Tags;
    auto query = task.query<
      const CommandRow,
      const StatEffect::Owner,
      const StatEffect::Target,
      const StatEffect::CurveOutput<>,
      StatEffect::CurveInput<>
    >();
    auto ids = task.getIDResolver();
    auto resolver = task.getResolver<
      FloatRow<Pos, X>,
      FloatRow<Pos, Y>
    >();

    task.setCallback([query, ids, resolver](AppTaskArgs&) mutable {
      CachedRow<FloatRow<Pos, X>> srcPosX, dstPosX;
      CachedRow<FloatRow<Pos, Y>> srcPosY, dstPosY;
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands, owners, targets, curveOutputs, curveInputs] = query.get(t);
        for(size_t i = 0; i < commands->size(); ++i) {
          const auto self = owners->at(i);
          const auto target = targets->at(i);
          if(self == StableElementID::invalid() || target == StableElementID::invalid()) {
            continue;
          }
          const auto rawSelf = ids->uncheckedUnpack(self);
          const auto rawTarget = ids->uncheckedUnpack(target);

          const Command& cmd = commands->at(i);
          if(resolver->tryGetOrSwapAllRows(rawSelf, srcPosX, srcPosY) &&
            resolver->tryGetOrSwapAllRows(rawTarget, dstPosX, dstPosY)) {
            glm::vec2 src = TableAdapters::read(rawSelf.getElementIndex(), *srcPosX, *srcPosY);
            const glm::vec2 dst = TableAdapters::read(rawTarget.getElementIndex(), *dstPosX, *dstPosY);
            const float curveOutput = curveOutputs->at(i);

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

              TableAdapters::write(rawSelf.getElementIndex(), src, *srcPosX, *srcPosY);
            }
            else {
              //Reset curve input when reaching destination
              curveInputs->at(i) = 0.0f;
            }
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}