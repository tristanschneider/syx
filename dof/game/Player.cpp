#include "Precompile.h"
#include "Player.h"

#include "curve/CurveSolver.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "stat/AllStatEffects.h"

#include "GameMath.h"
#include "DebugDrawer.h"

#include "glm/gtx/norm.hpp"

namespace Player {
  using namespace Tags;

  void init(GameDB db) {
    db;
    PlayerConfig& player = TableAdapters::getConfig({ db }).game->player;
    player.linearForceCurve.params.duration = 0.4f;
    player.linearForceCurve.params.scale = 0.012f;
    player.linearForceCurve.function = CurveMath::getFunction(CurveMath::CurveType::ExponentialEaseOut);

    player.linearSpeedCurve.params.duration = 1.0f;
    player.linearSpeedCurve.params.scale = 0.1f;
    player.linearSpeedCurve.function = CurveMath::getFunction(CurveMath::CurveType::SineEaseIn);

    player.linearStoppingSpeedCurve.params.duration = 0.5f;
    player.linearStoppingSpeedCurve.params.scale = 0.1f;
    player.linearStoppingSpeedCurve.params.flipInput = true;
    player.linearStoppingSpeedCurve.function = CurveMath::getFunction(CurveMath::CurveType::QuadraticEaseIn);
  }

  using namespace Math;

  void _updatePlayerInput(PlayerAdapter players,
    const GameConfig& config,
    AreaForceStatEffectAdapter areaEffect,
    LambdaStatEffectAdapter lambda
  ) {
    PROFILE_SCOPE("simulation", "playerinput");
    for(size_t i = 0; i < players.input->size(); ++i) {
      PlayerInput& input = players.input->at(i);
      glm::vec2 move(input.mMoveX, input.mMoveY);

      constexpr float epsilon = 0.0001f;
      const bool hasMoveInput = glm::length2(move) > epsilon;
      const float rawDT = config.world.deltaTime;

      const glm::vec2 velocity(players.object.physics.linVelX->at(i), players.object.physics.linVelY->at(i));
      const float angVel = players.object.physics.angVel->at(i);
      Impulse impulse;
      constexpr CurveSolver::CurveUniforms curveUniforms{ 1 };
      float curveOutput{};
      CurveSolver::CurveVaryings curveVaryings{ &input.moveT, &curveOutput };

      const glm::vec2 pos(players.object.transform.posX->at(i), players.object.transform.posY->at(i));
      const glm::vec2 forward(players.object.transform.rotX->at(i), players.object.transform.rotY->at(i));
      const float speed = glm::length(velocity);

      const CurveDefinition* linearSpeedCurve = &config.player.linearSpeedCurve;
      const CurveDefinition* linearForceCurve = &config.player.linearForceCurve;
      const CurveDefinition* angularSpeedCurve = &config.player.angularSpeedCurve;
      const CurveDefinition* angularForceCurve = &config.player.angularForceCurve;
      float timeScale = 1.0f;
      glm::vec2 desiredForward = move;
      if(!hasMoveInput) {
        timeScale = -1.0f;
        linearSpeedCurve = &config.player.linearStoppingSpeedCurve;
        linearForceCurve = &config.player.linearStoppingForceCurve;
        angularSpeedCurve = &config.player.angularStoppingSpeedCurve;
        angularForceCurve = &config.player.angularStoppingForceCurve;

        //If stopping, the desired input direction opposes the current velocity, bringing the player to a stop
        if(speed > 0.00001f) {
          move = velocity * (-1.f/speed);
        }
      }
      const float linearDT = CurveSolver::getDeltaTime(*linearSpeedCurve, rawDT)*timeScale;
      input.moveT = CurveSolver::advanceTimeDT(input.moveT, linearDT);
      const float linearSpeedT = CurveSolver::solve(input.moveT, *linearSpeedCurve);
      const float linearSpeed = hasMoveInput ? linearSpeedT : linearSpeedT*speed;
      //Currently the two curves share the same dt meaning duration changes to the force curve would be ignored
      const float linearForce = CurveSolver::solve(input.moveT, *linearForceCurve);

      const float angularDT = CurveSolver::getDeltaTime(*angularSpeedCurve, rawDT)*timeScale;
      input.angularMoveT = CurveSolver::advanceTimeDT(input.angularMoveT, angularDT);
      const float angularSpeed = CurveSolver::solve(input.angularMoveT, *angularSpeedCurve);
      const float angularForce = CurveSolver::solve(input.angularMoveT, *angularForceCurve);

      constexpr Mass mass = computePlayerMass();

      Constraint c;
      c.jacobian.a.linear = move;
      c.objMass.a = mass;
      c.velocity.a.linear = velocity;
      //Don't apply a force against the desired move direction
      c.limits.lambdaMin = 0.0f;
      //Cap the amount of force that is allowed to be applied to try to satisfy the constraint
      //This prevents movement from fighting against or overpowering physics, assuming the cap isn't too high
      c.limits.lambdaMax = linearForce;
      //This is the target velocity the constraint is solving for along the input axis
      c.limits.bias = -linearSpeed;

      ConstraintImpulse ci = solveConstraint(c);
      impulse = ci.a;

      if(config.player.drawMove) {
        const size_t l = TableAdapters::addStatEffectsSharedLifetime(lambda.base, StatEffect::INSTANT, nullptr, 1);
        lambda.command->at(l) = [pos, c, velocity, impulse, move, linearSpeed](LambdaStatEffect::Args& args) {
          const float scale = 8.0f;
          auto debug = TableAdapters::getDebugLines(*args.db);
          DebugDrawer::drawVector(debug, pos, velocity*scale, { 1, 0, 0 });
          DebugDrawer::drawVector(debug, pos, -c.jacobian.a.linear*c.limits.bias*scale, { 0, 1, 0 });
          DebugDrawer::drawVector(debug, pos, impulse.linear*scale, { 0, 0, 1 });
        };
      }

      c = {};
      c.jacobian.a.angular = 1.0f;
      c.objMass.a = mass;
      c.velocity.a.angular = angVel;
      c.limits.lambdaMax = angularForce;
      c.limits.lambdaMin = -angularForce;
      //If there is move input, use the bias to rotate towards the desired forward
      //Otherwise, use the constraint to use force to zero out the angular velocity
      if(hasMoveInput) {
        const float sinError = cross(forward, desiredForward);
        const float cosError = glm::dot(forward, desiredForward);
        const float angularError = std::atan2f(sinError, cosError);
        c.limits.bias = -angularError*angularSpeed;
      }

      ci = solveConstraint(c);
      impulse.angular += ci.a.angular;

      if(impulse.linear != glm::vec2{ 0 } || impulse.angular) {
        players.object.physics.linImpulseX->at(i) = impulse.linear.x;
        players.object.physics.linImpulseY->at(i) = impulse.linear.y;
        players.object.physics.angImpulse->at(i) = impulse.angular;
      }

      if(input.mAction1) {
        input.mAction1 = false;
        const size_t effect = TableAdapters::addStatEffectsSharedLifetime(areaEffect.base, config.ability.explodeLifetime, nullptr, 1);
        AreaForceStatEffect::Command& cmd = areaEffect.command->at(effect);
        cmd.origin = glm::vec2{ players.object.transform.posX->at(i), players.object.transform.posY->at(i) };
        cmd.direction = glm::vec2{ players.object.transform.rotX->at(i), players.object.transform.rotY->at(i) };
        cmd.dynamicPiercing = 3.0f;
        cmd.terrainPiercing = 0.0f;
        cmd.rayCount = 4;
        AreaForceStatEffect::Command::Cone cone;
        cone.halfAngle = 0.25f;
        cone.length = 15.0f;
        cmd.shape = cone;
      }
    }
  }

  TaskRange updateInput(GameDB db) {
    auto task = TaskNode::create([db](enki::TaskSetPartition, uint32_t thread) {
      _updatePlayerInput(
        TableAdapters::getGameplayPlayer(db),
        *TableAdapters::getConfig({ db }).game,
        TableAdapters::getAreaForceEffects(db, thread),
        TableAdapters::getLambdaEffects(db, thread)
      );
    });
    return TaskBuilder::addEndSync(task);
  }
}