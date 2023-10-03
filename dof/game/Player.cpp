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

  void init(RuntimeDatabaseTaskBuilder&& task) {
    Config::GameConfig* config = TableAdapters::getGameConfigMutable(task);

    Config::PlayerConfig& player = config->player;
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
    const SceneState* scene = task.query<const SharedRow<SceneState>>().tryGetSingletonElement();

    task.setCallback([scene, cameras, cameraModifier, players, playerModifier, config](AppTaskArgs& args) mutable {
      if(scene->mState != SceneState::State::SetupScene) {
        return;
      }

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

  void updateInput(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("player input");
    const Config::GameConfig* config = TableAdapters::getGameConfig(task);
    auto players = task.query<
      Row<PlayerInput>,
      const FloatRow<GLinVel, X>, const FloatRow<GLinVel, Y>,
      const FloatRow<GAngVel, Angle>,
      const FloatRow<GPos, X>, FloatRow<GPos, Y>,
      const FloatRow<GRot, CosAngle>, FloatRow<GRot, SinAngle>,
      FloatRow<GLinImpulse, X>, FloatRow<GLinImpulse, Y>,
      FloatRow<GAngImpulse, Angle>
    >();
    auto debug = TableAdapters::getDebugLines(task);

    task.setCallback([players, config, debug](AppTaskArgs& args) mutable {
      for(size_t t = 0; t < players.size(); ++t) {
        auto&& [input, linVelX, linVelY, angVel, posX, posY, rotX, rotY, impulseX, impulseY, impulseA] = players.get(t);
        for(size_t i = 0; input->size(); ++i) {
          PlayerInput& playerInput = input->at(i);
          glm::vec2 move(playerInput.mMoveX, playerInput.mMoveY);

          constexpr float epsilon = 0.0001f;
          const bool hasMoveInput = glm::length2(move) > epsilon;
          const float rawDT = config->world.deltaTime;

          const glm::vec2 velocity(TableAdapters::read(i, *linVelX, *linVelY));
          const float aVel = angVel->at(i);
          Impulse impulse;
          constexpr CurveSolver::CurveUniforms curveUniforms{ 1 };
          float curveOutput{};
          CurveSolver::CurveVaryings curveVaryings{ &playerInput.moveT, &curveOutput };

          const glm::vec2 pos(TableAdapters::read(i, *posX, *posY));
          const glm::vec2 forward(TableAdapters::read(i, *rotX, *rotY));
          const float speed = glm::length(velocity);

          const CurveDefinition* linearSpeedCurve = &Config::getCurve(config->player.linearSpeedCurve);
          const CurveDefinition* linearForceCurve = &Config::getCurve(config->player.linearForceCurve);
          const CurveDefinition* angularSpeedCurve = &Config::getCurve(config->player.angularSpeedCurve);
          const CurveDefinition* angularForceCurve = &Config::getCurve(config->player.angularForceCurve);
          float timeScale = 1.0f;
          glm::vec2 desiredForward = move;
          if(!hasMoveInput) {
            timeScale = -1.0f;
            linearSpeedCurve = &Config::getCurve(config->player.linearStoppingSpeedCurve);
            linearForceCurve = &Config::getCurve(config->player.linearStoppingForceCurve);
            angularSpeedCurve = &Config::getCurve(config->player.angularStoppingSpeedCurve);
            angularForceCurve = &Config::getCurve(config->player.angularStoppingForceCurve);

            //If stopping, the desired input direction opposes the current velocity, bringing the player to a stop
            if(speed > 0.00001f) {
              move = velocity * (-1.f/speed);
            }
          }

          const float linearDT = CurveSolver::getDeltaTime(*linearSpeedCurve, rawDT)*timeScale;
          playerInput.moveT = CurveSolver::advanceTimeDT(playerInput.moveT, linearDT);
          const float linearSpeedT = CurveSolver::solve(playerInput.moveT, *linearSpeedCurve);
          const float linearSpeed = hasMoveInput ? linearSpeedT : linearSpeedT*speed;
          //Currently the two curves share the same dt meaning duration changes to the force curve would be ignored
          const float linearForce = CurveSolver::solve(playerInput.moveT, *linearForceCurve);

          const float angularDT = CurveSolver::getDeltaTime(*angularSpeedCurve, rawDT)*timeScale;
          playerInput.angularMoveT = CurveSolver::advanceTimeDT(playerInput.angularMoveT, angularDT);
          const float angularSpeed = CurveSolver::solve(playerInput.angularMoveT, *angularSpeedCurve);
          const float angularForce = CurveSolver::solve(playerInput.angularMoveT, *angularForceCurve);

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

          if(config->player.drawMove) {
            const float scale = 8.0f;
            DebugDrawer::drawVector(debug, pos, velocity*scale, { 1, 0, 0 });
            DebugDrawer::drawVector(debug, pos, -c.jacobian.a.linear*c.limits.bias*scale, { 0, 1, 0 });
            DebugDrawer::drawVector(debug, pos, impulse.linear*scale, { 0, 0, 1 });
          }

          c = {};
          c.jacobian.a.angular = 1.0f;
          c.objMass.a = mass;
          c.velocity.a.angular = aVel;
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
            TableAdapters::add(i, impulse.linear, *impulseX, *impulseY);
            TableAdapters::add(i, impulse.angular, *impulseA);
          }

          Ability::TriggerResult shouldTrigger = Ability::DontTrigger{};
          if(playerInput.ability1) {
            const bool inputDown = playerInput.mAction1 == KeyState::Triggered || playerInput.mAction1 == KeyState::Down;
            Ability::AbilityInput& ability = *playerInput.ability1;
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
          advanceState(playerInput.mAction1);
          advanceState(playerInput.mAction2);
          std::visit([&](const auto& t) {
            if(t.resetInput) {
              playerInput.mAction1 = KeyState::Up;
            }
          }, shouldTrigger);

          AreaForceStatEffectAdapter areaEffect = TableAdapters::getAreaForceEffects(args);
          if(const auto withPower = std::get_if<Ability::TriggerWithPower>(&shouldTrigger)) {
            const size_t effect = TableAdapters::addStatEffectsSharedLifetime(areaEffect.base, StatEffect::INSTANT, nullptr, 1);
            AreaForceStatEffect::Command& cmd = areaEffect.command->at(effect);
            cmd.origin = TableAdapters::read(i, *posX, *posY);
            cmd.direction = TableAdapters::read(i, *rotX, *rotY);
            cmd.dynamicPiercing = config->ability.pushAbility.dynamicPiercing;
            cmd.terrainPiercing = config->ability.pushAbility.terrainPiercing;
            cmd.rayCount = config->ability.pushAbility.rayCount;
            AreaForceStatEffect::Command::Cone cone;
            cone.halfAngle = config->ability.pushAbility.coneHalfAngle;
            cone.length = config->ability.pushAbility.coneLength;
            cmd.shape = cone;
            cmd.damage = withPower->damage;
            AreaForceStatEffect::Command::FlatImpulse impulseType{ withPower->power };
            cmd.impulseType = impulseType;
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}