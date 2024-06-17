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
#include "ConstraintSolver.h"
#include "SpatialQueries.h"

namespace Player {
  using namespace Tags;

  void init(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("init player");
    Config::GameConfig* config = TableAdapters::getGameConfigMutable(task);

    task.setCallback([config](AppTaskArgs&) {
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
    });

    builder.submitTask(std::move(task));
  }

  void initAbility(Config::GameConfig& config, QueryResultRow<GameInput::PlayerInputRow>& input) {
    for(auto&& row : input) {
      for(GameInput::PlayerInput& in : *row) {
        *in.ability1 = Config::getAbility(config.ability.pushAbility.ability);
        in.wantsRebuild = true;
      }
    }
  }

  using namespace Math;

  //Feeds the on ground state into the input state machine as if it were a button being pressed
  //This allows input state transitions based on on ground state
  void senseGround(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("sense ground");
    auto players = task.query<
      const StableIDRow,
      GameInput::PlayerInputRow,
      GameInput::StateMachineRow>();
    auto sq = SpatialQuery::createReader(task);

    task.setCallback([players, sq](AppTaskArgs&) mutable {
      for(size_t t = 0; t < players.size(); ++t) {
        auto [stable, input, machines] = players.get(t);
        for(size_t i = 0; i < input->size(); ++i) {
          const auto self = StableElementID::fromStableRow(i, *stable);
          sq->begin(self);
          bool isOnGround{};
          //If colliding with something on the Z axis upwards, that counts as the ground
          while(const SpatialQuery::Result* result = sq->tryIterate()) {
            if(auto contactZ = std::get_if<SpatialQuery::ContactZ>(&result->contact)) {
              if(contactZ->normal > 0.0f) {
                isOnGround = true;
                break;
              }
            }
          }

          Input::StateMachine& machine = machines->at(i);
          constexpr auto og = GameInput::Keys::GAME_ON_GROUND;
          const Input::InputMapper& mapper = machine.getMapper();
          machine.traverse(isOnGround ? mapper.onPassthroughKeyDown(og) : mapper.onPassthroughKeyUp(og));
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void updateInput(IAppBuilder& builder) {
    senseGround(builder);

    auto task = builder.createTask();
    task.setName("player input");
    const Config::GameConfig* config = TableAdapters::getGameConfig(task);
    auto players = task.query<
      GameInput::PlayerInputRow,
      const GameInput::StateMachineRow,
      const FloatRow<GLinVel, X>, const FloatRow<GLinVel, Y>,
      const FloatRow<GAngVel, Angle>,
      const FloatRow<GPos, X>, FloatRow<GPos, Y>,
      const FloatRow<GRot, CosAngle>, FloatRow<GRot, SinAngle>,
      FloatRow<GLinImpulse, X>, FloatRow<GLinImpulse, Y>, FloatRow<GLinImpulse, Z>,
      FloatRow<GAngImpulse, Angle>
    >();
    auto debug = TableAdapters::getDebugLines(task);

    task.setCallback([players, config, debug](AppTaskArgs& args) mutable {
      for(size_t t = 0; t < players.size(); ++t) {
        auto&& [input, machines, linVelX, linVelY, angVel, posX, posY, rotX, rotY, impulseX, impulseY, impulseZ, impulseA] = players.get(t);
        for(size_t i = 0; i < input->size(); ++i) {
          GameInput::PlayerInput& playerInput = input->at(i);
          const Input::StateMachine& sm = machines->at(i);
          glm::vec2 move{ sm.getAbsoluteAxis2D(playerInput.nodes.move2D) };
          const float moveLen = glm::length(move);
          if(moveLen > 1.0f) {
            move /= moveLen;
          }

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

          //TODO: read from SharedMassRow
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

          std::vector<Ability::TriggerResult> triggers;
          for(const Input::Event& event : sm.readEvents()) {
            switch(event.id) {
            case GameInput::Events::CHANGE_DENSITY:
              //TODO:
              break;
            case GameInput::Events::JUMP: {
              //TODO: tie to config
              TableAdapters::add(i, 0.1f, *impulseZ);
              break;
            }
            case GameInput::Events::PARTIAL_TRIGGER_ACTION_1:
              //Partial trigger means the charge time is the time the button was held down
              if(playerInput.ability1) {
                triggers.push_back(Ability::tryTriggerDirectly(playerInput.ability1->trigger, GameInput::timespanToSeconds(event.timeInNode)));
              }
              break;
            case GameInput::Events::FULL_TRIGGER_ACTION_1:
              if(playerInput.ability1) {
                //Full trigger means time in node was min time plus time spent afterwards still holding
                triggers.push_back(
                  Ability::tryTriggerDirectly(
                    playerInput.ability1->trigger,
                    Ability::getMinChargeTime(playerInput.ability1->trigger) + GameInput::timespanToSeconds(event.timeInNode)
                  )
                );
              }
              break;
            }
          }

          AreaForceStatEffectAdapter areaEffect = TableAdapters::getAreaForceEffects(args);
          for(Ability::TriggerResult& shouldTrigger : triggers) {
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
      }
    });

    builder.submitTask(std::move(task));
  }
}