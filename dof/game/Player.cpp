#include "Precompile.h"
#include "Player.h"

#include "ability/PlayerAbility.h"
#include "curve/CurveSolver.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "stat/AllStatEffects.h"

#include "GameMath.h"
#include "DebugDrawer.h"
#include <random>
#include "glm/gtx/norm.hpp"
#include "AppBuilder.h"

namespace Player {
  using namespace Tags;

  void init(GameDB db) {
    Config::PlayerConfig& player = TableAdapters::getConfig({ db }).game->player;
    auto& force = Config::getCurve(player.linearForceCurve);
    force.params.duration = 0.4f;
    force.params.scale = 0.012f;
    force.function = CurveMath::getFunction(CurveMath::CurveType::ExponentialEaseOut);

    auto& speed = Config::getCurve(player.linearSpeedCurve);
    speed.params.duration = 1.0f;
    speed.params.scale = 0.1f;
    speed.function = CurveMath::getFunction(CurveMath::CurveType::SineEaseIn);

    auto& stopping = Config::getCurve(player.linearStoppingSpeedCurve);
    stopping.params.duration = 0.5f;
    stopping.params.scale = 0.1f;
    stopping.params.flipInput = true;
    stopping.function = CurveMath::getFunction(CurveMath::CurveType::QuadraticEaseIn);
  }

  void setupScene(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Player setup");
    auto cameras = task.query<Row<Camera>, const StableIDRow>();
    std::shared_ptr<ITableModifier> cameraModifier = task.getModifierForTable(cameras.matchingTableIDs.front());
    auto players = task.query<FloatRow<Pos, X>, FloatRow<Pos, Y>, Row<PlayerInput>, const StableIDRow>();
    Config::GameConfig* config = task.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement();
    std::shared_ptr<ITableModifier> playerModifier = task.getModifierForTable(players.matchingTableIDs.front());

    task.setCallback([cameras, cameraModifier, players, playerModifier, config](AppTaskArgs& args) mutable {
      std::random_device device;
      std::mt19937 generator(device());
      cameraModifier->resize(1);
      const size_t cameraIndex = 0;
      Camera& mainCamera = cameras.get<0>(0).at(cameraIndex);
      const size_t cameraStableId = cameras.get<1>(0).at(cameraIndex);
      mainCamera.zoom = 15.f;

      playerModifier->resize(1);
      const StableIDRow& playerStableRow = players.get<const StableIDRow>(0);
      //TODO: this could be built into the modifier itself
      const size_t playerIndex = 0;
      Events::onNewElement(StableElementID::fromStableRow(playerIndex, playerStableRow), args);

      //Random angle in sort of radians
      const float playerStartAngle = float(generator() % 360)*6.282f/360.0f;
      const float playerStartDistance = 25.0f;
      //Start way off the screen, the world boundary will fling them into the scene
      players.get<0>(0).at(0) = playerStartDistance*std::cos(playerStartAngle);
      players.get<1>(0).at(0) = playerStartDistance*std::sin(playerStartAngle);

      //Make the camera follow the player
      auto follow = TableAdapters::getFollowTargetByPositionEffects(args);
      const size_t id = TableAdapters::addStatEffectsSharedLifetime(follow.base, StatEffect::INFINITE, &cameraStableId, 1);
      follow.command->at(id).mode = FollowTargetByPositionStatEffect::FollowMode::Interpolation;
      follow.base.target->at(id) = StableElementID::fromStableID(playerStableRow.at(playerIndex));
      follow.base.curveDefinition->at(id) = &Config::getCurve(config->camera.followCurve);

      //Load ability from config
      initAbility(*config, std::get<2>(players.rows));
    });
    builder.submitTask(std::move(task));
  }

  void initAbility(Config::GameConfig& config, QueryResultRow<Row<PlayerInput>>& input) {
    for(auto&& row : input) {
      for(PlayerInput& in : *row) {
        *in.ability1 = Config::getAbility(config.ability.pushAbility.ability);
      }
    }
  }

  using namespace Math;

  void _updatePlayerInput(PlayerAdapter players,
    const Config::GameConfig& config,
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

      const CurveDefinition* linearSpeedCurve = &Config::getCurve(config.player.linearSpeedCurve);
      const CurveDefinition* linearForceCurve = &Config::getCurve(config.player.linearForceCurve);
      const CurveDefinition* angularSpeedCurve = &Config::getCurve(config.player.angularSpeedCurve);
      const CurveDefinition* angularForceCurve = &Config::getCurve(config.player.angularForceCurve);
      float timeScale = 1.0f;
      glm::vec2 desiredForward = move;
      if(!hasMoveInput) {
        timeScale = -1.0f;
        linearSpeedCurve = &Config::getCurve(config.player.linearStoppingSpeedCurve);
        linearForceCurve = &Config::getCurve(config.player.linearStoppingForceCurve);
        angularSpeedCurve = &Config::getCurve(config.player.angularStoppingSpeedCurve);
        angularForceCurve = &Config::getCurve(config.player.angularStoppingForceCurve);

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

      Ability::TriggerResult shouldTrigger = Ability::DontTrigger{};
      if(input.ability1) {
        const bool inputDown = input.mAction1 == KeyState::Triggered || input.mAction1 == KeyState::Down;
        Ability::AbilityInput& ability = *input.ability1;
        if(!Ability::isOnCooldown(ability.cooldown)) {
          shouldTrigger = Ability::tryTrigger(ability.trigger, { rawDT, inputDown });
        }

        const bool abilityActive = std::get_if<Ability::DontTrigger>(&shouldTrigger) == nullptr;
        Ability::updateCooldown(ability.cooldown, { rawDT, abilityActive });
      }

      //Hack to advance input here so gameplay doesn't miss it. Should work on the input layer somehow
      auto advanceState = [](KeyState& state) {
        switch(state) {
        case KeyState::Triggered: state = KeyState::Down; break;
        case KeyState::Released: state = KeyState::Up; break;
        }
      };
      advanceState(input.mAction1);
      advanceState(input.mAction2);
      std::visit([&](const auto& t) {
        if(t.resetInput) {
          input.mAction1 = KeyState::Up;
        }
      }, shouldTrigger);

      if(const auto withPower = std::get_if<Ability::TriggerWithPower>(&shouldTrigger)) {
        const size_t effect = TableAdapters::addStatEffectsSharedLifetime(areaEffect.base, StatEffect::INSTANT, nullptr, 1);
        AreaForceStatEffect::Command& cmd = areaEffect.command->at(effect);
        cmd.origin = glm::vec2{ players.object.transform.posX->at(i), players.object.transform.posY->at(i) };
        cmd.direction = glm::vec2{ players.object.transform.rotX->at(i), players.object.transform.rotY->at(i) };
        cmd.dynamicPiercing = config.ability.pushAbility.dynamicPiercing;
        cmd.terrainPiercing = config.ability.pushAbility.terrainPiercing;
        cmd.rayCount = config.ability.pushAbility.rayCount;
        AreaForceStatEffect::Command::Cone cone;
        cone.halfAngle = config.ability.pushAbility.coneHalfAngle;
        cone.length = config.ability.pushAbility.coneLength;
        cmd.shape = cone;
        cmd.damage = withPower->damage;
        AreaForceStatEffect::Command::FlatImpulse impulseType{ withPower->power };
        cmd.impulseType = impulseType;
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