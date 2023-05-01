#include "Precompile.h"
#include "stat/VelocityStatEffect.h"

#include "Simulation.h"

namespace StatEffect {
  TaskRange processStat(VelocityStatEffectTable& table, GameDB db) {
    auto task = TaskNode::create([&table, db](...) {
      auto& commands = std::get<VelocityStatEffect::CommandRow>(table.mRows);
      const auto& owners = std::get<StatEffect::Owner>(table.mRows);
      for(size_t i = 0; i < commands.size(); ++i) {
        const auto self = owners.at(i).toPacked<GameDatabase>();
        if(!self.isValid()) {
          continue;
        }

        const VelocityStatEffect::VelocityCommand& cmd = commands.at(i);
        auto linearX = Queries::getRowInTable<FloatRow<Tags::LinVel, Tags::X>>(db.db, self);
        auto linearY = Queries::getRowInTable<FloatRow<Tags::LinVel, Tags::Y>>(db.db, self);
        auto angular = Queries::getRowInTable<FloatRow<Tags::AngVel, Tags::Angle>>(db.db, self);
        if(linearX && linearY) {
          linearX->at(self.getElementIndex()) += cmd.linearImpulse.x;
          linearY->at(self.getElementIndex()) += cmd.linearImpulse.y;
        }
        if(angular) {
          angular->at(self.getElementIndex()) += cmd.angularImpulse;
        }
      }
    });
    return TaskBuilder::addEndSync(task);
  }
};