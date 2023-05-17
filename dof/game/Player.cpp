#include "Precompile.h"
#include "Player.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "stat/AllStatEffects.h"

#include "glm/gtx/norm.hpp"

namespace Player {
  using namespace Tags;

  void _updatePlayerInput(PlayerAdapter players,
    const GameConfig& config,
    AreaForceStatEffectAdapter areaEffect
  ) {
    PROFILE_SCOPE("simulation", "playerinput");
    for(size_t i = 0; i < players.input->size(); ++i) {
      PlayerInput& input = players.input->at(i);
      glm::vec2 move(input.mMoveX, input.mMoveY);
      const float speed = config.playerSpeed;
      move *= speed;

      const glm::vec2 velocity(players.object.physics.linVelX->at(i), players.object.physics.linVelY->at(i));
      glm::vec2 impulse{ 0 };

      const float maxStoppingForce = config.playerMaxStoppingForce;
      //Apply a stopping force if there is no input. This is a flat amount so it doesn't negate physics
      const float epsilon = 0.0001f;
      const float velocityLen2 = glm::length2(velocity);
      if(glm::length2(move) < epsilon && velocityLen2 > epsilon) {
        //Apply an impulse in the opposite direction of velocity up to maxStoppingForce without exceeding velocity
        const float velocityLen = std::sqrt(velocityLen2);
        const float stoppingAmount = std::min(maxStoppingForce, velocityLen);
        const float stoppingMultiplier = stoppingAmount/velocityLen;
        impulse -= velocity*stoppingMultiplier;
      }
      //Apply an impulse in the desired move direction
      else {
        impulse += move;
      }

      if(impulse != glm::vec2{ 0 }) {
        players.object.physics.linImpulseX->at(i) = impulse.x;
        players.object.physics.linImpulseY->at(i) = impulse.y;
      }

      if(input.mAction1) {
        input.mAction1 = false;
        const size_t effect = TableAdapters::addStatEffectsSharedLifetime(areaEffect.base, config.explodeLifetime, nullptr, 1);
        areaEffect.pointX->at(effect) = players.object.transform.posX->at(i);
        areaEffect.pointY->at(effect) = players.object.transform.posY->at(i);
        areaEffect.strength->at(effect) = config.explodeStrength;
      }
    }
  }

  TaskRange updateInput(GameDB db) {
    auto task = TaskNode::create([db](enki::TaskSetPartition, uint32_t thread) {
      _updatePlayerInput(
        TableAdapters::getPlayer(db),
        *TableAdapters::getConfig({ db }).game,
        TableAdapters::getAreaForceEffects(db, thread)
      );
    });
    return TaskBuilder::addEndSync(task);
  }
}