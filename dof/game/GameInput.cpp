#include "Precompile.h"
#include "GameInput.h"

#include "AppBuilder.h"
#include "ability/PlayerAbility.h"

namespace GameInput {
  PlayerInput::PlayerInput()
    : ability1(std::make_unique<Ability::AbilityInput>()) {
  }

  PlayerInput::PlayerInput(PlayerInput&&) = default;
  PlayerInput& PlayerInput::operator=(PlayerInput&&) = default;

  PlayerInput::~PlayerInput() = default;

  Input::Timespan secondsToTimespan(float seconds) {
    constexpr float simulationRate = 1.0f/60.0f;
    return static_cast<Input::Timespan>(seconds * simulationRate);
  }

  float timespanToSeconds(Input::Timespan time) {
    return static_cast<float>(time * 60);
  }

  //Creates the mappings for an ability with nodes going from abilityRoot out to trigger the event,
  //wait for the cooldown, then return to root when the cooldown has elapsed
  struct AbilityVisitor {
    struct Output {
      Input::NodeIndex triggerNode{};
    };

    void operator()(const Ability::InstantTrigger&) {
      output.triggerNode = builder.addNode(Input::NodeBuilder{}.button().emitEvent(fullTriggerEvent));
      builder.addEdge(Input::EdgeBuilder{}
        .from(abilityRoot)
        .to(output.triggerNode)
        .keyDown(key)
      );
    }

    void operator()(const Ability::ChargeTrigger& t) {
      auto charging = builder.addNode(Input::NodeBuilder{}.button());
      builder.addEdge(Input::EdgeBuilder{}
        .from(abilityRoot)
        .to(charging)
        .keyDown(key)
      );
      auto chargeComplete = builder.addNode(Input::NodeBuilder{});
      auto fullTrigger = builder.addNode(Input::NodeBuilder{}.emitEvent(fullTriggerEvent));
      output.triggerNode = builder.addNode(Input::NodeBuilder{});
      auto chargeAbort = builder.addNode(Input::NodeBuilder{}.emitEvent(partialTriggerEvent));
      //Key up while in charging state before entering chargeComplete means a partial trigger
      builder.addEdge(Input::EdgeBuilder{}
        .from(charging)
        .to(chargeAbort)
        .keyUp(key)
      );
      //Abort fires the event then immediately moves to the final trigger node
      builder.addEdge(Input::EdgeBuilder{}
        .from(chargeAbort)
        .to(output.triggerNode)
        .unconditional()
      );
      //Waiting for the cooldown to elapse moves to the charge complete node, which doesn't trigger the event yet
      builder.addEdge(Input::EdgeBuilder{}
        .from(charging)
        .to(chargeComplete)
        .timeout(secondsToTimespan(t.minimumCharge))
      );
      //From the complete node the event triggers when the player releases the key
      builder.addEdge(Input::EdgeBuilder{}
        .from(chargeComplete)
        .to(fullTrigger)
        .keyUp(key)
      );
      //Trigger moves unconditionally to the same final node that the partial trigger goes to
      builder.addEdge(Input::EdgeBuilder{}
        .from(fullTrigger)
        .to(output.triggerNode)
        .unconditional()
      );
    }

    void operator()(const Ability::DisabledCooldown& c) {
      //Wait for the cooldown then go back to the root node so the ability can trigger again
      builder.addEdge(Input::EdgeBuilder{}
        .from(output.triggerNode)
        .to(abilityRoot)
        .timeout(secondsToTimespan(c.maxTime))
      );
    }

    Input::NodeIndex abilityRoot{};
    Input::KeyMapID key{};
    Input::EventID partialTriggerEvent{};
    Input::EventID fullTriggerEvent{};
    Input::StateMachineBuilder& builder;
    Output output;
  };

  void addAndGoto(Input::StateMachineBuilder& machine, Input::NodeIndex thenNode, Input::EdgeData edge) {
    machine.addEdge(edge);
    machine.addEdge(Input::EdgeBuilder{}
      .from(edge.to)
      .to(thenNode)
      .unconditional()
    );
  }

  void rebuildStateMachine(PlayerInput& input, Input::StateMachine& machine, const Input::InputMapper& mapper) {
    using namespace Input;
    StateMachineBuilder builder;
    const NodeIndex root = StateMachine::ROOT_NODE;
    const NodeIndex move = builder.addNode(NodeBuilder{}.axis2D());
    const NodeIndex changeDensity = builder.addNode(NodeBuilder{}.axis1D().emitEvent(Events::CHANGE_DENSITY));

    //Abilities
    if(input.ability1) {
      const NodeIndex ability1Root = builder.addNode(NodeBuilder{});
      builder.addEdge(EdgeBuilder{}
        .from(root)
        .to(ability1Root)
        .keyDown(Keys::INIT_ONCE)
      );
      AbilityVisitor visit1{
        ability1Root,
        Keys::ACTION_1,
        Events::PARTIAL_TRIGGER_ACTION_1,
        Events::FULL_TRIGGER_ACTION_1,
        builder
      };
      std::visit(visit1, input.ability1->trigger);
      std::visit(visit1, input.ability1->cooldown);
    }

    //Directional inputs
    addAndGoto(builder, root, EdgeBuilder{}
      .from(root)
      .to(move)
      .anyDelta2D(Keys::MOVE_2D)
      .forkState()
    );
    addAndGoto(builder, root, EdgeBuilder{}
      .from(root)
      .to(changeDensity)
      .anyDelta1D(Keys::CHANGE_DENSITY_1D)
      .forkState()
    );

    machine = StateMachine{ std::move(builder), InputMapper{ mapper } };
    input.nodes.move2D = move;

    machine.traverse(machine.getMapper().onPassthroughKeyDown(Keys::INIT_ONCE));
  }

  void buildDebugCameraMachine(Input::StateMachine& machine, const Input::InputMapper& mapper) {
    using namespace Input;
    StateMachineBuilder builder;
    const NodeIndex root = builder.addNode(NodeBuilder{});
    const NodeIndex zoom = builder.addNode(NodeBuilder{}.axis1D().emitEvent(Events::DEBUG_ZOOM));
    const NodeIndex repeat = builder.addNode(NodeBuilder{});

    builder.addEdge(EdgeBuilder{}
      .from(StateMachine::ROOT_NODE)
      .to(root)
      .keyDown(Keys::INIT_ONCE)
    );
    builder.addEdge(EdgeBuilder{}
      .from(root)
      .to(zoom)
      .anyDelta1D(Keys::DEBUG_ZOOM_1D)
    );
    //TODO: this edge order is unintuitive, it's backwards because builder adds to front of list
    //Repeat event while axis is nonzero
    builder.addEdge(EdgeBuilder{}
      .from(zoom)
      .to(repeat)
      .unconditional()
    );
    builder.addEdge(EdgeBuilder{}
      .from(repeat)
      .to(zoom)
      .unconditional()
      .consumeEvent()
    );

    //Exit when axis goes back to zero
    builder.addEdge(EdgeBuilder{}
      .from(zoom)
      .to(root)
      .absolute1D(Keys::DEBUG_ZOOM_1D, 0.0f, 0.0f)
      .consumeEvent()
    );

    //addAndGoto(builder, root, EdgeBuilder{}
    //  .from(root)
    //  .to(zoom)
    //  .anyDelta1D(Keys::DEBUG_ZOOM_1D)
    //  .forkState()
    //);
    machine = StateMachine{ std::move(builder), InputMapper{ mapper } };
    machine.traverse(machine.getMapper().onPassthroughKeyDown(Keys::INIT_ONCE));
  }

  void buildDebugCameraMachine(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("build camera SM");
    auto query = task.query<StateMachineRow, CameraDebugInputRow>();
    const Input::InputMapper* mapper = task.query<const GlobalMappingsRow>().tryGetSingletonElement();
    task.setCallback([query, mapper](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [machines, cam] = query.get(t);
        for(size_t i = 0; i < machines->size(); ++i) {
          if(cam->at(i).needsInit) {
            buildDebugCameraMachine(machines->at(i), *mapper);
            cam->at(i).needsInit = false;
          }
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  void advanceStateMachines(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("advance input SM");
    auto query = task.query<StateMachineRow>();
    task.setCallback([query](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [machines] = query.get(t);
        for(Input::StateMachine& machine : *machines) {
          machine.clearEvents();
          machine.traverse(machine.getMapper().onTick(1));
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  void processPlayerRebuild(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("player build SM");
    auto query = task.query<StateMachineRow, PlayerInputRow>();
    const Input::InputMapper* mapper = task.query<const GlobalMappingsRow>().tryGetSingletonElement();
    task.setCallback([query, mapper](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [machines, playerInputs] = query.get(t);
        for(size_t i = 0; i < machines->size(); ++i) {
          PlayerInput& input = playerInputs->at(i);
          if(input.wantsRebuild) {
            rebuildStateMachine(input, machines->at(i), *mapper);
            input.wantsRebuild = false;
          }
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  //TODO: make it possible to build only once
  void init(IAppBuilder&) {
    //buildDebugCameraMachine(builder);
  }

  void update(IAppBuilder& builder) {
    processPlayerRebuild(builder);
    buildDebugCameraMachine(builder);
    advanceStateMachines(builder);
  }
}