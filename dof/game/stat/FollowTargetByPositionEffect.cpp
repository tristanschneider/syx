#include "Precompile.h"
#include "stat/FollowTargetByPositionEffect.h"

#include "AllStatEffects.h"
#include "glm/glm.hpp"

#include "AppBuilder.h"
#include <transform/TransformRows.h>

namespace FollowTargetByPositionStatEffect {
  RuntimeTable& getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::getStatTable<FollowTargetByPositionStatEffect::CommandRow>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase(getArgs(args), args.getLocalDB())
  {
    command = table.tryGet<CommandRow>();
    target = table.tryGet<StatEffect::Target>();
    curve = table.tryGet<StatEffect::CurveDef<>>();
  }

  Builder& Builder::setMode(FollowMode mode) {
    for(auto i : currentEffects) {
      command->at(i).mode = mode;
    }
    return *this;
  }

  Builder& Builder::setTarget(const ElementRef& ref) {
    for(auto i : currentEffects) {
      target->at(i) = ref;
    }
    return *this;
  }

  Builder& Builder::setCurve(CurveDefinition& c) {
    for(auto i : currentEffects) {
      curve->at(i) = &c;
    }
    return *this;
  }

  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("FollowTargetByPosition Stat");
    auto query = task.query<
      const CommandRow,
      const StatEffect::Owner,
      const StatEffect::Target,
      const StatEffect::CurveOutput<>,
      StatEffect::CurveInput<>
    >();
    auto res = task.getRefResolver();
    auto resolver = task.getResolver<
      Transform::WorldTransformRow,
      Transform::TransformNeedsUpdateRow
    >();

    task.setCallback([query, res, resolver](AppTaskArgs&) mutable {
      CachedRow<Transform::WorldTransformRow> srcTransform;
      CachedRow<const Transform::WorldTransformRow> dstTransform;
      CachedRow<Transform::TransformNeedsUpdateRow> srcUpdate;

      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands, owners, targets, curveOutputs, curveInputs] = query.get(t);
        for(size_t i = 0; i < commands->size(); ++i) {
          const auto self = res.tryUnpack(owners->at(i));
          const auto target = res.tryUnpack(targets->at(i));
          if(!self || !target) {
            continue;
          }
          const auto rawSelf = *self;
          const auto rawTarget = *target;

          const Command& cmd = commands->at(i);
          if(resolver->tryGetOrSwapAllRows(rawSelf, srcTransform, srcUpdate) &&
            resolver->tryGetOrSwapAllRows(rawTarget, dstTransform)) {
            Transform::PackedTransform& srcT = srcTransform->at(rawSelf.getElementIndex());
            const Transform::PackedTransform& dstT = dstTransform->at(rawTarget.getElementIndex());
            glm::vec2 src = srcT.pos2();
            const glm::vec2 dst = dstT.pos2();
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

              srcT.setPos(src);
              srcUpdate->getOrAdd(rawSelf.getElementIndex());
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