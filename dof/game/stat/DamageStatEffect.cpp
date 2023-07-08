#include "Precompile.h"

#include "stat/DamageStatEffect.h"

#include "Simulation.h"
#include "TableAdapters.h"

namespace StatEffect {
  TaskRange processStat(DamageStatEffectTable& table, GameDB db) {
    auto task = TaskNode::create([&table, db](...) {
      auto& commands = std::get<DamageStatEffect::CommandRow>(table.mRows);
      const auto& owners = std::get<StatEffect::Owner>(table.mRows);
      for(size_t i = 0; i < commands.size(); ++i) {
        const auto self = owners.at(i).toPacked<GameDatabase>();
        if(!self.isValid()) {
          continue;
        }
        FragmentAdapter fragments = TableAdapters::getFragmentsInTable(db, self.getTableIndex());
        const DamageStatEffect::Command& cmd = commands.at(i);
        const size_t selfI = self.getElementIndex();
        if(fragments.damageTaken) {
          float& currentDamage = fragments.damageTaken->at(selfI);
          currentDamage += cmd.damage;
          //Indicate damage through alpha channel of tint
          if(fragments.tint) {
            float& alpha = fragments.tint->at(selfI).a;
            alpha = 1.0f - std::min(1.0f, currentDamage / 100.0f);
          }
        }
      }
    });
    return TaskBuilder::addEndSync(task);
  }
}