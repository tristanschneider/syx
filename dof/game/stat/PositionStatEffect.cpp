#include "Precompile.h"
#include "stat/PositionStatEffect.h"

#include "AllStatEffects.h"
#include "AppBuilder.h"
#include "Simulation.h"
#include "TableAdapters.h"

namespace PositionStatEffect {
  RuntimeTable& getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::getStatTable<PositionStatEffect::CommandRow>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase{ getArgs(args) }
  {
    command = table.tryGet<CommandRow>();
  }

  Builder& Builder::setZ(float z) {
    for(auto i : currentEffects) {
      command->at(i).posZ = z;
    }
    return *this;
  }

  Builder& Builder::setPos(const glm::vec2& p) {
    for(auto i : currentEffects) {
      command->at(i).pos = p;
    }
    return *this;
  }

  Builder& Builder::setRot(const glm::vec2& r) {
    for(auto i : currentEffects) {
      command->at(i).rot = r;
    }
    return *this;
  }

  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("position stat");
    auto query = task.query<
      const CommandRow,
      const StatEffect::Owner
    >();
    auto ids = task.getIDResolver();
    using namespace Tags;

    auto resolver = task.getResolver<
      FloatRow<Pos, X>, FloatRow<Pos, Y>, FloatRow<Pos, Z>,
      FloatRow<Rot, CosAngle>, FloatRow<Rot, SinAngle>
    >();

    task.setCallback([query, ids, resolver](AppTaskArgs&) mutable {
      CachedRow<FloatRow<Pos, X>> px;
      CachedRow<FloatRow<Pos, Y>> py;
      CachedRow<FloatRow<Pos, Z>> pz;
      CachedRow<FloatRow<Rot, CosAngle>> rx;
      CachedRow<FloatRow<Rot, SinAngle>> ry;
      auto res = ids->getRefResolver();

      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands, owners] = query.get(t);
        for(size_t i = 0; i < commands->size(); ++i) {
          const auto owner = res.tryUnpack(owners->at(i));
          if(!owner) {
            continue;
          }
          const UnpackedDatabaseElementID self = *owner;
          const size_t si = self.getElementIndex();

          const PositionStatEffect::PositionCommand& cmd = commands->at(i);
          if(cmd.pos && resolver->tryGetOrSwapAllRows(self, px, py)) {
            TableAdapters::write(si, *cmd.pos, *px, *py);
          }
          if(cmd.rot && resolver->tryGetOrSwapAllRows(self, rx, ry)) {
            TableAdapters::write(si, *cmd.rot, *rx, *ry);
          }
          if(cmd.posZ && resolver->tryGetOrSwapRow(pz, self)) {
            pz->at(si) = *cmd.posZ;
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}