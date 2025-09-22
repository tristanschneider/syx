#include "Precompile.h"
#include "stat/VelocityStatEffect.h"

#include "AppBuilder.h"
#include "Physics.h"
#include "Simulation.h"
#include "TableAdapters.h"

#include "AllStatEffects.h"

namespace VelocityStatEffect {
  RuntimeTable& getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::getStatTable<VelocityStatEffect::CommandRow>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase{ getArgs(args), args.getLocalDB() }
  {
    command = table.tryGet<CommandRow>();
  }

  Builder& Builder::addImpulse(const ImpulseCommand& cmd) {
    for(auto i : currentEffects) {
      command->at(i) = { cmd };
    }
    return *this;
  }

  Builder& Builder::setZ(const SetZCommand& cmd) {
    for(auto i : currentEffects) {
      command->at(i) = { cmd };
    }
    return *this;
  }

  struct Processor {
    void operator()(const ImpulseCommand& cmd) {
      if(resolver.tryGetOrSwapAllRows(self, vx, vy)) {
        TableAdapters::add(self.getElementIndex(), cmd.linearImpulse, *vx, *vy);
      }
      if(cmd.impulseZ && resolver.tryGetOrSwapRow(vz, self)) {
        TableAdapters::add(self.getElementIndex(), cmd.impulseZ, *vz);
      }
      if(resolver.tryGetOrSwapAllRows(self, va)) {
        TableAdapters::add(self.getElementIndex(), cmd.angularImpulse, *va);
      }
    }

    void operator()(const SetZCommand& cmd) {
      if(resolver.tryGetOrSwapRow(vz, self)) {
        vz->at(self.getElementIndex()) = cmd.z;
      }
    }

    ITableResolver& resolver;
    CachedRow<VelX> vx;
    CachedRow<VelY> vy;
    CachedRow<VelZ> vz;
    CachedRow<VelA> va;
    UnpackedDatabaseElementID self;
  };

  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("velocity stat");
    auto query = task.query<
      const StatEffect::Owner,
      const VelocityStatEffect::CommandRow
    >();
    using namespace Tags;
    auto resolver = task.getResolver<VelX, VelY, VelZ, VelA>();
    auto ids = task.getIDResolver();

    task.setCallback([query, resolver, ids](AppTaskArgs&) mutable {
      Processor processor{ *resolver };
      auto res = ids->getRefResolver();

      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [owners, commands] = query.get(t);
        for(size_t i = 0; i < owners->size(); ++i) {
          const auto owner = res.tryUnpack(owners->at(i));
          if(!owner) {
            continue;
          }

          processor.self = *owner;
          std::visit(processor, commands->at(i).data);
        }
      }
    });

    builder.submitTask(std::move(task));
  }
};