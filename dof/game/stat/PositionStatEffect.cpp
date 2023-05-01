#include "Precompile.h"
#include "stat/PositionStatEffect.h"

#include "Simulation.h"

namespace StatEffect {
  TaskRange processStat(PositionStatEffectTable& table, GameDB db) {
    auto task = TaskNode::create([&table, db](...) {
      auto& commands = std::get<PositionStatEffect::CommandRow>(table.mRows);
      const auto& owners = std::get<StatEffect::Owner>(table.mRows);
      for(size_t i = 0; i < commands.size(); ++i) {
        const auto self = owners.at(i).toPacked<GameDatabase>();
        if(!self.isValid()) {
          continue;
        }

        const PositionStatEffect::PositionCommand& cmd = commands.at(i);
        if(cmd.pos) {
          auto posX = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::X>>(db.db, self);
          auto posY = Queries::getRowInTable<FloatRow<Tags::Pos, Tags::Y>>(db.db, self);
          posX->at(self.getElementIndex()) = cmd.pos->x;
          posY->at(self.getElementIndex()) = cmd.pos->y;
        }
        if(cmd.rot) {
          auto rotX = Queries::getRowInTable<FloatRow<Tags::Rot, Tags::CosAngle>>(db.db, self);
          auto rotY = Queries::getRowInTable<FloatRow<Tags::Rot, Tags::SinAngle>>(db.db, self);
          rotX->at(self.getElementIndex()) = cmd.rot->x;
          rotY->at(self.getElementIndex()) = cmd.rot->y;
        }
      }
    });
    return TaskBuilder::addEndSync(task);
  }
}