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

#include "Constraints.h"
namespace Player {
  using namespace Tags;

  constexpr Constraints::ConstraintDefinitionKey MOTOR_KEY{ 0 };

  void configurePlayerMotor(IAppBuilder& builder) {
    auto task = builder.createTask();
    auto q = task.query<Constraints::TableConstraintDefinitionsRow, const IsPlayer>();
    task.setCallback([q](AppTaskArgs&) mutable {
      for(size_t t = 0; t < q.size(); ++t) {
        auto [definitions, _] = q.get(t);
        Constraints::Definition def;
        def.common = def.common.create();
        def.sideA = def.sideA.create();
        def.targetA = Constraints::SelfTarget{};
        def.targetB = Constraints::NoTarget{};
        def.joint = def.joint.create();
        def.storage = def.storage.create();
        definitions->at().definitions.push_back(def);
      }
    });
    builder.submitTask(std::move(task.setName("init player motor")));
  }

  void init(IAppBuilder& builder) {
    configurePlayerMotor(builder);

    auto task = builder.createTask();
    task.setName("init player");
    Config::GameConfig* config = TableAdapters::getGameConfigMutable(task);

    task.setCallback([config](AppTaskArgs&) {
      Config::PlayerConfig& player = config->player;
      auto& force = Config::getCurve(player.linearForceCurve);
      force.params.duration = 0.4f;
      force.params.scale = 0.21f;
      force.function = CurveMath::getFunction(CurveMath::CurveType::One);

      auto& speed = Config::getCurve(player.linearSpeedCurve);
      speed.params.duration = 0.691f;
      speed.params.scale = 0.162f;
      speed.function = CurveMath::getFunction(CurveMath::CurveType::ElasticEaseOut);

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
          const auto self = stable->at(i);
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
      const FloatRow<GPos, X>, FloatRow<GPos, Y>,
      const FloatRow<GRot, CosAngle>, FloatRow<GRot, SinAngle>
    >();
    auto debug = TableAdapters::getDebugLines(task);
    Constraints::Builder motorBuilder{ Constraints::Definition::resolve(task, players.matchingTableIDs[0], MOTOR_KEY) };

    task.setCallback([players, config, debug, motorBuilder](AppTaskArgs& args) mutable {
      for(size_t t = 0; t < players.size(); ++t) {
        auto&& [input, machines, posX, posY, rotX, rotY] = players.get(t);
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

          constexpr CurveSolver::CurveUniforms curveUniforms{ 1 };
          float curveOutput{};
          CurveSolver::CurveVaryings curveVaryings{ &playerInput.moveT, &curveOutput };

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
          }

          const float linearDT = CurveSolver::getDeltaTime(*linearSpeedCurve, rawDT)*timeScale;
          playerInput.moveT = CurveSolver::advanceTimeDT(playerInput.moveT, linearDT);
          const float linearSpeed = CurveSolver::solve(playerInput.moveT, *linearSpeedCurve);
          //The values are all so small that they're annoying to edit in the curve editor. Shift them down so the UI works better
          constexpr float scalar = 0.1f;
          //Currently the two curves share the same dt meaning duration changes to the force curve would be ignored
          const float linearForce = CurveSolver::solve(playerInput.moveT, *linearForceCurve);

          const float angularDT = CurveSolver::getDeltaTime(*angularSpeedCurve, rawDT)*timeScale;
          playerInput.angularMoveT = CurveSolver::advanceTimeDT(playerInput.angularMoveT, angularDT);
          const float angularSpeed = CurveSolver::solve(playerInput.angularMoveT, *angularSpeedCurve);
          const float angularForce = CurveSolver::solve(playerInput.angularMoveT, *angularForceCurve)*scalar;

          motorBuilder.select({ i });
          Constraints::MotorJoint joint;
          joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::WorldSpaceLinear))
            .set(gnx::enumCast(Constraints::MotorJoint::Flags::CanPull));
          joint.linearForce = linearForce;
          //Target is zero when there is no input, meaning the constraint will reduce velocity to zero, like friciton
          joint.linearTarget = move * linearSpeed;
          joint.angularForce = angularForce;
          if(hasMoveInput) {
            joint.angularTarget = Geo::angleFromDirection(desiredForward);
            joint.flags.set(gnx::enumCast(Constraints::MotorJoint::Flags::AngularOrientationTarget));
            joint.biasScalar = angularSpeed;
          }
          else {
            //When there is no input, turn the local space target to zero so it stops rotating
            joint.angularTarget = 0;
            joint.flags.reset(gnx::enumCast(Constraints::MotorJoint::Flags::AngularOrientationTarget));
          }

          std::vector<Ability::TriggerResult> triggers;
          for(const Input::Event& event : sm.readEvents()) {
            switch(event.id) {
            case GameInput::Events::CHANGE_DENSITY:
              //TODO:
              break;
            case GameInput::Events::JUMP: {
              //TODO: tie to config
              joint.linearTargetZ = joint.zForce = 0.1f;
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

          motorBuilder.setJointType({ joint });

          AreaForceStatEffect::Builder ab{ args };
          for(Ability::TriggerResult& shouldTrigger : triggers) {
            if(const auto withPower = std::get_if<Ability::TriggerWithPower>(&shouldTrigger)) {
              ab.createStatEffects(1).setLifetime(StatEffect::INSTANT);
              ab.setOrigin(TableAdapters::read(i, *posX, *posY))
                .setDirection(TableAdapters::read(i, *rotX, *rotY))
                .setPiercing(config->ability.pushAbility.dynamicPiercing, config->ability.pushAbility.terrainPiercing)
                .setRayCount(config->ability.pushAbility.rayCount);
              AreaForceStatEffect::Command::Cone cone;
              cone.halfAngle = config->ability.pushAbility.coneHalfAngle;
              cone.length = config->ability.pushAbility.coneLength;
              ab.setShape({ cone })
                .setDamage(withPower->damage)
                .setImpulse(AreaForceStatEffect::Command::FlatImpulse{ withPower->power });
            }
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}