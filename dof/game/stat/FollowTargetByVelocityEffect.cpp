#include "Precompile.h"
#include "stat/FollowTargetByVelocityEffect.h"

#include "AllStatEffects.h"
#include "glm/glm.hpp"
#include "Simulation.h"
#include "TableAdapters.h"
#include "GameMath.h"
#include "AppBuilder.h"

namespace FollowTargetByVelocityStatEffect {
  auto getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::createBuilderBase<FollowTargetByVelocityStatEffectTable>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase(getArgs(args))
    , command{ &std::get<CommandRow>(getArgs(args).table.mRows) }
    , target{ &std::get<StatEffect::Target>(getArgs(args).table.mRows) }
  {
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

  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("FollowTargetByVelocity Stat");
    auto ids = task.getIDResolver();
    auto query = task.query<
      const CommandRow,
      const StatEffect::Owner,
      const StatEffect::Target
    >();
    using namespace Tags;
    auto resolver = task.getResolver<
      FloatRow<GLinImpulse, X>, FloatRow<GLinImpulse, Y>,
      const FloatRow<Pos, X>, const FloatRow<Pos, Y>
    >();

    task.setCallback([ids, query, resolver](AppTaskArgs&) mutable {
      CachedRow<FloatRow<GLinImpulse, X>> impulseX;
      CachedRow<FloatRow<GLinImpulse, Y>> impulseY;
      CachedRow<const FloatRow<Pos, X>> srcPosX, dstPosX;
      CachedRow<const FloatRow<Pos, Y>> srcPosY, dstPosY;
      auto res = ids->getRefResolver();
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands, owners, targets] = query.get(t);
        for(size_t i = 0; i < commands->size(); ++i) {
          const auto self = res.tryUnpack(owners->at(i));
          const auto target = res.tryUnpack(targets->at(i));
          if(!self || !target) {
            continue;
          }
          const auto rawSelf = *self;
          const auto rawTarget = *target;

          if(resolver->tryGetOrSwapAllRows(rawSelf, impulseX, impulseY, srcPosX, srcPosY) &&
            resolver->tryGetOrSwapAllRows(rawTarget, dstPosX, dstPosY)) {
            const size_t selfI = rawSelf.getElementIndex();
            const size_t targetI = rawTarget.getElementIndex();
            const Command& cmd = commands->at(i);

            VisitArgs args {
              TableAdapters::read(selfI, *srcPosX, *srcPosY),
              TableAdapters::read(targetI, *dstPosX, *dstPosY)
            };
            const Math::Impulse impulse = std::visit([&](const auto& c) { return visitComputeImpulse(c, args); }, cmd.mode);

            TableAdapters::add(selfI, impulse.linear, *impulseX, *impulseY);
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}