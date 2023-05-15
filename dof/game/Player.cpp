#include "Precompile.h"
#include "Player.h"

#include "Simulation.h"
#include "TableAdapters.h"

#include "glm/gtx/norm.hpp"

namespace Player {
  using namespace Tags;

  void _updatePlayerInput(PlayerTable& players, GlobalPointForceTable& pointForces, const GameConfig& config) {
    PROFILE_SCOPE("simulation", "playerinput");
    for(size_t i = 0; i < TableOperations::size(players); ++i) {
      PlayerInput& input = std::get<Row<PlayerInput>>(players.mRows).at(i);
      glm::vec2 move(input.mMoveX, input.mMoveY);
      const float speed = config.playerSpeed;
      move *= speed;

      float& vx = std::get<FloatRow<LinVel, X>>(players.mRows).at(i);
      float& vy = std::get<FloatRow<LinVel, Y>>(players.mRows).at(i);
      glm::vec2 velocity(vx, vy);

      const float maxStoppingForce = config.playerMaxStoppingForce;
      //Apply a stopping force if there is no input. This is a flat amount so it doesn't negate physics
      const float epsilon = 0.0001f;
      const float velocityLen2 = glm::length2(velocity);
      if(glm::length2(move) < epsilon && velocityLen2 > epsilon) {
        //Apply an impulse in the opposite direction of velocity up to maxStoppingForce without exceeding velocity
        const float velocityLen = std::sqrt(velocityLen2);
        const float stoppingAmount = std::min(maxStoppingForce, velocityLen);
        const float stoppingMultiplier = stoppingAmount/velocityLen;
        velocity -= velocity*stoppingMultiplier;
      }
      //Apply an impulse in the desired move direction
      else {
        velocity += move;
      }

      vx = velocity.x;
      vy = velocity.y;

      if(input.mAction1) {
        input.mAction1 = false;
        const size_t lifetime = config.explodeLifetime;
        const float strength = config.explodeStrength;
        const size_t f = TableOperations::size(pointForces);
        TableOperations::addToTable(pointForces);
        std::get<FloatRow<Tags::Pos, Tags::X>>(pointForces.mRows).at(f) = std::get<FloatRow<Tags::Pos, Tags::X>>(players.mRows).at(i);
        std::get<FloatRow<Tags::Pos, Tags::Y>>(pointForces.mRows).at(f) = std::get<FloatRow<Tags::Pos, Tags::Y>>(players.mRows).at(i);
        std::get<ForceData::Strength>(pointForces.mRows).at(f) = strength;
        std::get<ForceData::Lifetime>(pointForces.mRows).at(f) = lifetime;
      }
    }
  }

  TaskRange updateInput(GameDB db) {
    auto task = TaskNode::create([db](...) {
      _updatePlayerInput(std::get<PlayerTable>(db.db.mTables), std::get<GlobalPointForceTable>(db.db.mTables), *TableAdapters::getConfig({ db }).game);
    });
    return TaskBuilder::addEndSync(task);
  }
}