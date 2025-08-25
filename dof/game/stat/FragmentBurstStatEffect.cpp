#include "Precompile.h"
#include "stat/FragmentBurstStatEffect.h"

#include "AllStatEffects.h"
#include "AppBuilder.h"
#include "Simulation.h"
#include "SpatialQueries.h"
#include "TableAdapters.h"
#include "stat/AreaForceStatEffect.h"
#include <transform/TransformRows.h>
#include <math/Geometric.h>

namespace FragmentBurstStatEffect {
  RuntimeTable& getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::getStatTable<FragmentBurstStatEffect::CommandRow>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase{ getArgs(args), args.getLocalDB() }
  {
    command = table.tryGet<CommandRow>();
  }

  Builder& Builder::setRadius(float radius) {
    for(auto i : currentEffects) {
      command->at(i).radius = radius;
    }
    return *this;
  }

  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("fragment burst stat");
    auto query = task.query<
      const StatEffect::Owner,
      const CommandRow,
      StatEffect::Lifetime
    >();
    using namespace Tags;
    auto resolver = task.getResolver<
      const Transform::WorldTransformRow,
      const CanTriggerFragmentBurstRow
    >();
    auto ids = task.getIDResolver();
    auto reader = SpatialQuery::createReader(task);

    task.setCallback([query, resolver, ids, reader](AppTaskArgs& args) mutable {
      CachedRow<const CanTriggerFragmentBurstRow> canTriggerBurst;
      CachedRow<const Transform::WorldTransformRow> transforms;
      ElementRefResolver idResolver = ids->getRefResolver();
      AreaForceStatEffect::Builder burst{ args };
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [owners, commands, lifetime] = query.get(t);
        for(size_t i = 0; i < owners->size(); ++i) {
          const ElementRef& ownerRef = owners->at(i);
          const auto owner = idResolver.tryUnpack(owners->at(i));
          if(!owner) {
            continue;
          }

          //See if any nearby objects can trigger the burst
          reader->begin(ownerRef);
          bool triggerBurst{};
          while(const SpatialQuery::Result* result = reader->tryIterate()) {
            if(auto otherID = idResolver.tryUnpack(result->other); result->isCollision() && otherID && resolver->tryGetOrSwapRow(canTriggerBurst, *otherID)) {
              triggerBurst = true;
              break;
            }
          }

          if(triggerBurst) {
            const auto self = *owner;
            if(!resolver->tryGetOrSwapAllRows(self, transforms)) {
              continue;
            }

            const glm::vec2 pos = transforms->at(self.getElementIndex()).pos2();
            printf("trigger burst\n");
            //Trigger removal of this effect
            lifetime->at(i) = 0;
            //Queue the burst itself
            burst.createStatEffects(1).setLifetime(StatEffect::INSTANT);
            const Command& cmd = commands->at(i);
            burst
              //TODO: uncomment this when dislodging fragments doesn't cause heap corruption
              //.setDislodgeFragments()
              .setOrigin(pos)
              .setDirection(glm::vec2{ 1, 0 })
              //Actually a circle but that's not implemented so using a very wide cone
              .setShape(AreaForceStatEffect::Command::Cone{ Constants::PI, cmd.radius })
              .setRayCount(360)
              //TODO: make configurable
              .setImpulse(AreaForceStatEffect::Command::FlatImpulse{ 0.0005f })
              .setPiercing(cmd.radius / 3.0f, cmd.radius / 2.0f);
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
};