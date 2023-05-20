#include "Precompile.h"
#include "Player.h"

#include "curve/CurveSolver.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "stat/AllStatEffects.h"

#include "glm/gtx/norm.hpp"

namespace Player {
  using namespace Tags;

  void init(GameDB db) {
    PlayerConfig& player = TableAdapters::getConfig({ db }).game->player;
    player.linearMoveCurve.params.duration = 0.4f;
    player.linearMoveCurve.params.scale = 0.012f;
    player.linearMoveCurve.function = CurveMath::getFunction(CurveMath::CurveType::ExponentialEaseOut);

    player.angularMoveCurve.params.duration = 1.0f;
    player.angularMoveCurve.params.scale = 0.1f;
    player.angularMoveCurve.function = CurveMath::getFunction(CurveMath::CurveType::SineEaseIn);

    player.linearStoppingCurve.params.duration = 0.5f;
    player.linearStoppingCurve.params.scale = 0.1f;
    player.linearStoppingCurve.params.flipInput = true;
    player.linearStoppingCurve.function = CurveMath::getFunction(CurveMath::CurveType::QuadraticEaseIn);
  }

  void _updatePlayerInput(PlayerAdapter players,
    const GameConfig& config,
    AreaForceStatEffectAdapter areaEffect
  ) {
    PROFILE_SCOPE("simulation", "playerinput");
    for(size_t i = 0; i < players.input->size(); ++i) {
      PlayerInput& input = players.input->at(i);
      glm::vec2 move(input.mMoveX, input.mMoveY);

      constexpr float epsilon = 0.0001f;
      const bool hasMoveInput = glm::length2(move) > epsilon;
      const float rawDT = config.world.deltaTime;

      const glm::vec2 velocity(players.object.physics.linVelX->at(i), players.object.physics.linVelY->at(i));
      glm::vec2 impulse{ 0 };
      constexpr CurveSolver::CurveUniforms curveUniforms{ 1 };
      float curveOutput{};
      CurveSolver::CurveVaryings curveVaryings{ &input.moveT, &curveOutput };

      //Apply a stopping force if there is no input. This is a flat amount so it doesn't negate physics
      if(!hasMoveInput) {
        const CurveDefinition& curve = config.player.linearStoppingCurve;
        //Advance time backwards along curve when motion is stopping
        const float dt = -CurveSolver::getDeltaTime(curve, rawDT);
        CurveSolver::advanceTimeDT(dt, curveUniforms, curveVaryings);
        input.moveT = curveOutput;
        CurveSolver::solve(curve, curveUniforms, curveVaryings);

        const float velocityLen2 = glm::length2(velocity);
        if(velocityLen2 > epsilon) {
          //Apply an impulse in the opposite direction of velocity up to maxStoppingForce without exceeding velocity
          const float velocityLen = std::sqrt(velocityLen2);
          const float stoppingAmount = std::min(curveOutput, velocityLen);
          const float stoppingMultiplier = stoppingAmount/velocityLen;
          impulse -= velocity*stoppingMultiplier;
        }
      }
      //Apply an impulse in the desired move direction
      else {
        const CurveDefinition& curve = config.player.linearMoveCurve;
        //Advance time forwards along curve when motion is progress
        const float dt = CurveSolver::getDeltaTime(curve, rawDT);
        CurveSolver::advanceTimeDT(dt, curveUniforms, curveVaryings);
        input.moveT = curveOutput;
        CurveSolver::solve(curve, curveUniforms, curveVaryings);

        impulse += move*curveOutput;
      }

      if(impulse != glm::vec2{ 0 }) {
        players.object.physics.linImpulseX->at(i) = impulse.x;
        players.object.physics.linImpulseY->at(i) = impulse.y;
      }

      if(input.mAction1) {
        input.mAction1 = false;
        const size_t effect = TableAdapters::addStatEffectsSharedLifetime(areaEffect.base, config.ability.explodeLifetime, nullptr, 1);
        areaEffect.pointX->at(effect) = players.object.transform.posX->at(i);
        areaEffect.pointY->at(effect) = players.object.transform.posY->at(i);
        areaEffect.strength->at(effect) = config.ability.explodeStrength;
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