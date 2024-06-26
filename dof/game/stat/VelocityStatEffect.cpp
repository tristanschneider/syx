#include "Precompile.h"
#include "stat/VelocityStatEffect.h"

#include "AppBuilder.h"
#include "Simulation.h"
#include "TableAdapters.h"

namespace VelocityStatEffect {
  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("velocity stat");
    auto query = task.query<
      const StatEffect::Owner,
      const VelocityStatEffect::CommandRow
    >();
    using namespace Tags;
    auto resolver = task.getResolver<
      FloatRow<LinVel, X>, FloatRow<LinVel, Y>, FloatRow<LinVel, Z>,
      FloatRow<AngVel, Angle>
    >();
    auto ids = task.getIDResolver();

    task.setCallback([query, resolver, ids](AppTaskArgs&) mutable {
      CachedRow<FloatRow<LinVel, X>> vx;
      CachedRow<FloatRow<LinVel, Y>> vy;
      CachedRow<FloatRow<LinVel, Z>> vz;
      CachedRow<FloatRow<AngVel, Angle>> va;

      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [owners, commands] = query.get(t);
        for(size_t i = 0; i < owners->size(); ++i) {
          const StableElementID& owner = owners->at(i);
          if(owner == StableElementID::invalid()) {
            continue;
          }

          const auto self = ids->uncheckedUnpack(owner);
          const VelocityCommand& cmd = commands->at(i);
          if(resolver->tryGetOrSwapAllRows(self, vx, vy)) {
            TableAdapters::add(self.getElementIndex(), cmd.linearImpulse, *vx, *vy);
          }
          if(cmd.impulseZ && resolver->tryGetOrSwapRow(vz, self)) {
            TableAdapters::add(self.getElementIndex(), cmd.impulseZ, *vz);
          }
          if(resolver->tryGetOrSwapAllRows(self, va)) {
            TableAdapters::add(self.getElementIndex(), cmd.angularImpulse, *va);
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
};