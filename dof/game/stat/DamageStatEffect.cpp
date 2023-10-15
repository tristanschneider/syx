#include "Precompile.h"

#include "stat/DamageStatEffect.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "AppBuilder.h"

namespace DamageStatEffect {
  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Damage Stat");
    auto ids = task.getIDResolver();
    auto query = task.query<
      const CommandRow,
      const StatEffect::Owner
    >();
    auto resolver = task.getResolver<
      DamageTaken,
      Tint
    >();

    task.setCallback([ids, query, resolver](AppTaskArgs&) mutable {
      CachedRow<DamageTaken> damage;
      CachedRow<Tint> tint;
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands, owners] = query.get(t);
        for(size_t i = 0; i < commands->size(); ++i) {
          const auto self = owners->at(i);
          if(self == StableElementID::invalid()) {
            continue;
          }
          const auto rawSelf = ids->uncheckedUnpack(self);
          const size_t selfI = rawSelf.getElementIndex();
          if(resolver->tryGetOrSwapAllRows(rawSelf, damage)) {
            const Command& cmd = commands->at(i);
            float& currentDamage = damage->at(selfI);
            currentDamage += cmd.damage;
            //Indicate damage through alpha channel of tint
            if(resolver->tryGetOrSwapAllRows(rawSelf, tint)) {
              float& alpha = tint->at(selfI).a;
              alpha = 1.0f - std::min(1.0f, currentDamage / 100.0f);
            }
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}