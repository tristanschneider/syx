#include "Precompile.h"

#include "stat/DamageStatEffect.h"

#include "AllStatEffects.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "AppBuilder.h"

namespace DamageStatEffect {
  RuntimeTable& getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::getStatTable<DamageStatEffect::CommandRow>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase(getArgs(args), args.getLocalDB())
  {
    command = table.tryGet<CommandRow>();
  }

  Builder& Builder::setDamage(float damage) {
    for(auto i : currentEffects) {
      command->at(i).damage = damage;
    }
    return *this;
  }

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
      auto res = ids->getRefResolver();
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands, owners] = query.get(t);
        for(size_t i = 0; i < commands->size(); ++i) {
          const auto self = res.tryUnpack(owners->at(i));
          if(!self) {
            continue;
          }
          const auto rawSelf = *self;
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