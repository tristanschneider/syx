#include "Precompile.h"
#include "stat/PositionStatEffect.h"

#include "AppBuilder.h"
#include "Simulation.h"
#include "TableAdapters.h"

namespace PositionStatEffect {
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

      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands, owners] = query.get(t);
        for(size_t i = 0; i < commands->size(); ++i) {
          const StableElementID& owner = owners->at(i);
          if(owner == StableElementID::invalid()) {
            continue;
          }
          const UnpackedDatabaseElementID self = ids->uncheckedUnpack(owner);
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