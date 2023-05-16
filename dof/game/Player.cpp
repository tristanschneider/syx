#include "Precompile.h"
#include "Player.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "stat/AllStatEffects.h"

#include "glm/gtx/norm.hpp"

namespace Player {
  using namespace Tags;

  //Modify thread locals
  //Read gameplay extracted values
  //Write PlayerInput
  void _updatePlayerInput(PlayerAdapter players,
    const GameConfig& config,
    LambdaStatEffectAdapter lambdaEffect
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
        const size_t effectID = lambdaEffect.command->size();
        auto& modifier = lambdaEffect.base.modifier;
        modifier.modifier.resize(modifier.table, effectID + 1, *modifier.stableMappings);
        lambdaEffect.base.owner->at(effectID) = StableElementID::fromStableRow(i, *players.object.stable);
        lambdaEffect.base.lifetime->at(effectID) = StatEffect::INSTANT;
        lambdaEffect.command->at(effectID) = [](LambdaStatEffect::Args& args) {
          const GameConfig& config = *TableAdapters::getConfig(*args.db).game;
          GlobalPointForceTable& pointForces = std::get<GlobalPointForceTable>(args.db->db.mTables);
          PlayerAdapter players = TableAdapters::getPlayer(*args.db);
          const size_t pid = GameDatabase::ElementID{ args.resolvedID.mUnstableIndex }.getElementIndex();

          const size_t lifetime = config.explodeLifetime;
          const float strength = config.explodeStrength;
          const size_t f = TableOperations::size(pointForces);
          TableOperations::addToTable(pointForces);
          std::get<FloatRow<Tags::Pos, Tags::X>>(pointForces.mRows).at(f) = players.object.transform.posX->at(pid);
          std::get<FloatRow<Tags::Pos, Tags::Y>>(pointForces.mRows).at(f) = players.object.transform.posY->at(pid);
          std::get<ForceData::Strength>(pointForces.mRows).at(f) = strength;
          std::get<ForceData::Lifetime>(pointForces.mRows).at(f) = lifetime;
        };
      }
    }
  }

  TaskRange updateInput(GameDB db) {
    auto task = TaskNode::create([db](enki::TaskSetPartition, uint32_t thread) {
      _updatePlayerInput(
        TableAdapters::getPlayer(db),
        *TableAdapters::getConfig({ db }).game,
        TableAdapters::getLambdaEffects(db, thread)
      );
    });
    return TaskBuilder::addEndSync(task);
  }
}