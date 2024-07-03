#include "Precompile.h"
#include "stat/VelocityStatEffect.h"

#include "AppBuilder.h"
#include "Simulation.h"
#include "TableAdapters.h"

#include "AllStatEffects.h"

namespace VelocityStatEffect {
  auto getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::createBuilderBase<VelocityStatEffectTable>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase{ getArgs(args) }
    , command{ &std::get<CommandRow>(getArgs(args).table.mRows) }
  {
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
    CachedRow<FloatRow<Tags::LinVel, Tags::X>> vx;
    CachedRow<FloatRow<Tags::LinVel, Tags::Y>> vy;
    CachedRow<FloatRow<Tags::LinVel, Tags::Z>> vz;
    CachedRow<FloatRow<Tags::AngVel, Tags::Angle>> va;
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
    auto resolver = task.getResolver<
      FloatRow<LinVel, X>, FloatRow<LinVel, Y>, FloatRow<LinVel, Z>,
      FloatRow<AngVel, Angle>
    >();
    auto ids = task.getIDResolver();

    task.setCallback([query, resolver, ids](AppTaskArgs&) mutable {
      Processor processor{ *resolver };

      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [owners, commands] = query.get(t);
        for(size_t i = 0; i < owners->size(); ++i) {
          const StableElementID& owner = owners->at(i);
          if(owner == StableElementID::invalid()) {
            continue;
          }

          processor.self = ids->uncheckedUnpack(owner);
          std::visit(processor, commands->at(i).data);
        }
      }
    });

    builder.submitTask(std::move(task));
  }
};