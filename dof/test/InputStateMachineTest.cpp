#include "Precompile.h"
#include "CppUnitTest.h"

#include "InputStateMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace InputMappings {
  constexpr Input::KeyMapID MOVE{ 0 };
  constexpr Input::KeyMapID DEBUG_ZOOM{ 1 };
  constexpr Input::KeyMapID ACTION_1{ 2 };
  constexpr Input::KeyMapID ACTION_2{ 3 };
  constexpr Input::KeyMapID ENABLE_PLAYER_INPUT{ 4 };
  constexpr Input::KeyMapID DISABLE_PLAYER_INPUT{ 5 };

  struct Keys {
    Input::KeyMapID move{};
  };

  Keys createStateMachine(Input::StateMachineBuilder& machine);
};

namespace InputEvents {
  constexpr Input::EventID ACTION_1_BEGIN{ 0 };
  constexpr Input::EventID ACTION_1_TRIGGER{ 1 };
  constexpr Input::EventID ACTION_2_TRIGGER{ 2 };
  constexpr Input::EventID DEBUG_ZOOM{ 3 };
}

namespace InputMappings {
  void addAndExit(Input::StateMachineBuilder& machine, const Input::EdgeData& data) {
    machine.addEdge(data);
    machine.addEdge(Input::EdgeBuilder{}
      .from(data.to)
      .to(Input::StateMachine::ROOT_NODE)
      .unconditional()
    );
  }

  Keys createStateMachine(Input::StateMachineBuilder& machine) {
    //Having this as a root should make it possible to exit player control if desired by traversing out of the node
    //Kind of, any substates would need to finish
    auto playerRoot = machine.addNode({ Input::Node::Empty{} });
    auto move = machine.addNode({ Input::Node::Axis2D{} });
    auto zoom = machine.addNode(Input::NodeBuilder{}.axis1D().emitEvent(InputEvents::DEBUG_ZOOM));
    //Have this trigger with a charge up that executes on release
    auto action1Begin = machine.addNode(Input::NodeBuilder{}.button().emitEvent(InputEvents::ACTION_1_BEGIN));
    auto action1Hold = machine.addNode(Input::Node{ Input::Node::Button{} });
    auto action1Release = machine.addNode(Input::NodeBuilder{}.button().emitEvent(InputEvents::ACTION_1_TRIGGER));
    //Have this execute straight on trigger
    auto action2 = machine.addNode(Input::NodeBuilder{}.button().emitEvent(InputEvents::ACTION_2_TRIGGER));

    //Start on enable player input and go back on disable
    machine.addEdge(Input::EdgeBuilder{}
      .from(Input::StateMachine::ROOT_NODE)
      .to(playerRoot).keyDown(InputMappings::ENABLE_PLAYER_INPUT)
    );
    machine.addEdge(Input::EdgeBuilder{}
      .from(playerRoot)
      .to(Input::StateMachine::ROOT_NODE)
      .keyDown(InputMappings::DISABLE_PLAYER_INPUT)
    );
    //Fork into move which then exits after writing the move amount
    addAndExit(machine, Input::EdgeBuilder{}
      .from(playerRoot)
      .to(move)
      .anyDelta2D(InputMappings::MOVE)
      .forkState()
    );
    //Fork into zoom and exit
    addAndExit(machine, Input::EdgeBuilder{}
      .from(playerRoot)
      .to(zoom)
      .anyDelta1D(InputMappings::DEBUG_ZOOM)
      .forkState()
    );
    //Press
    machine.addEdge(Input::EdgeBuilder{}
      .from(playerRoot)
      .to(action1Begin)
      .keyDown(InputMappings::ACTION_1)
      .forkState()
    );
    //Hold
    machine.addEdge(Input::EdgeBuilder{}
      .from(action1Begin)
      .to(action1Hold)
      .unconditional()
    );
    addAndExit(machine, Input::EdgeBuilder{}
      .from(action1Hold)
      .to(action1Release)
      .keyUp(InputMappings::ACTION_1)
    );
    machine.addEdge(Input::EdgeBuilder{}
      .from(playerRoot)
      .to(action2)
      .keyDown(InputMappings::ACTION_2)
      .forkState()
    );
    machine.addEdge(Input::EdgeBuilder{}
      .from(action2)
      .to(Input::StateMachine::ROOT_NODE)
      .keyUp(InputMappings::ACTION_2)
    );

    Keys result;
    result.move = move;
    return result;
  }
}

namespace Test {
  namespace PlatformInput {
    constexpr Input::PlatformInputID W{ 10 };
    constexpr Input::PlatformInputID A{ 11 };
    constexpr Input::PlatformInputID S{ 12 };
    constexpr Input::PlatformInputID D{ 13 };
    constexpr Input::PlatformInputID Plus{ 14 };
    constexpr Input::PlatformInputID Minus{ 15 };
    constexpr Input::PlatformInputID A1{ 16 };
    constexpr Input::PlatformInputID A2{ 17 };
    constexpr Input::PlatformInputID LeftThumbstick{ 18 };
    constexpr Input::PlatformInputID A2Redundant{ 19 };
  }

  void buildPlatformMappings(Input::InputMapper& mapper) {
    mapper.addKeyAs2DRelativeMapping(PlatformInput::W, InputMappings::MOVE, glm::vec2{ 0, 1 });
    mapper.addKeyAs2DRelativeMapping(PlatformInput::A, InputMappings::MOVE, glm::vec2{ -1, 0 });
    mapper.addKeyAs2DRelativeMapping(PlatformInput::S, InputMappings::MOVE, glm::vec2{ 0, -1 });
    mapper.addKeyAs2DRelativeMapping(PlatformInput::D, InputMappings::MOVE, glm::vec2{ 1, 0 });
    mapper.addKeyAs1DRelativeMapping(PlatformInput::Plus, InputMappings::DEBUG_ZOOM, 1.0f);
    mapper.addKeyAs1DRelativeMapping(PlatformInput::Minus, InputMappings::DEBUG_ZOOM, -1.0f);
    mapper.addAxis2DMapping(PlatformInput::LeftThumbstick, InputMappings::MOVE);
    mapper.addKeyMapping(PlatformInput::A1, InputMappings::ACTION_1);
    mapper.addKeyMapping(PlatformInput::A2, InputMappings::ACTION_2);
    mapper.addKeyMapping(PlatformInput::A2Redundant, InputMappings::ACTION_2);
  }

  void assertEq(const glm::vec2& l, const glm::vec2& r) {
    constexpr float E = 0.0001f;
    Assert::AreEqual(l.x, r.x, E);
    Assert::AreEqual(l.y, r.y, E);
  }

  TEST_CLASS(InputStateMachineTest) {
    TEST_METHOD(Basic) {
      Input::InputMapper temp;
      buildPlatformMappings(temp);
      Input::StateMachineBuilder builder;
      InputMappings::Keys keys = InputMappings::createStateMachine(builder);
      Input::StateMachine machine{ std::move(builder), std::move(temp) };
      const Input::InputMapper& mapper = machine.getMapper();

      const std::vector<Input::Event>& events = machine.readEvents();

      //Root player node disabled so nothing should happen
      machine.traverse(mapper.onKeyDown(PlatformInput::A2));
      Assert::IsTrue(events.empty());

      machine.traverse(mapper.onPassthroughKeyDown(InputMappings::ENABLE_PLAYER_INPUT));

      //Trigger Action 2 and make sure it fires the event on key down
      machine.traverse(mapper.onKeyDown(PlatformInput::A2));
      Assert::AreEqual(size_t(1), events.size());
      Assert::AreEqual(InputEvents::ACTION_2_TRIGGER, events[0].id);
      machine.clearEvents();

      //Try to trigger action 2 via a redundant keybind. It should realize it's already pressed and do nothing
      machine.traverse(mapper.onKeyDown(PlatformInput::A2Redundant));
      Assert::IsTrue(events.empty());

      machine.traverse(mapper.onKeyUp(PlatformInput::A2));
      //Still shouldn't trigger because the other is held down
      machine.traverse(mapper.onKeyDown(PlatformInput::A2));
      Assert::IsTrue(events.empty());

      //Now release both and trigger through the redundant mapping
      machine.traverse(mapper.onKeyUp(PlatformInput::A2));
      machine.traverse(mapper.onKeyUp(PlatformInput::A2Redundant));
      machine.traverse(mapper.onKeyDown(PlatformInput::A2Redundant));
      Assert::AreEqual(size_t(1), events.size());
      Assert::AreEqual(InputEvents::ACTION_2_TRIGGER, events[0].id);
      machine.clearEvents();

      //Trigger Action 1 on and make sure it fires the event on release
      machine.traverse(mapper.onKeyDown(PlatformInput::A1));
      Assert::AreEqual(size_t(1), events.size());
      Assert::AreEqual(InputEvents::ACTION_1_BEGIN, events[0].id);
      machine.clearEvents();
      machine.traverse(mapper.onTick(2));
      machine.traverse(mapper.onKeyUp(PlatformInput::A1));
      Assert::AreEqual(size_t(1), events.size());
      Assert::AreEqual(InputEvents::ACTION_1_TRIGGER, events[0].id);
      Assert::AreEqual(Input::Timespan{ 2 }, events[0].timeInNode);
      machine.clearEvents();

      //1D input through key mapping
      machine.traverse(mapper.onKeyDown(PlatformInput::Plus));
      Assert::AreEqual(size_t(1), events.size());
      Assert::AreEqual(InputEvents::DEBUG_ZOOM, events[0].id);
      const Input::EdgeTraverser& traverser = events[0].traverser;
      Assert::AreEqual(1.0f, traverser.getAxis1DDelta());
      machine.clearEvents();

      //2D input direct
      machine.traverse(mapper.onAxis2DAbsolute(PlatformInput::LeftThumbstick, { 0.5f, 0.5f }));
      assertEq({ 0.5f, 0.5f }, machine.getAbsoluteAxis2D(keys.move));
      machine.traverse(mapper.onAxis2DRelative(PlatformInput::LeftThumbstick, { -0.5f, 0.0f }));
      assertEq({ 0.0f, 0.5f }, machine.getAbsoluteAxis2D(keys.move));

      //2D input through key mappings while thumbstick from above is still held
      machine.traverse(mapper.onKeyDown(PlatformInput::S));
      machine.traverse(mapper.onKeyDown(PlatformInput::A));
      assertEq({ -1, -0.5f }, machine.getAbsoluteAxis2D(keys.move));
      machine.traverse(mapper.onKeyUp(PlatformInput::S));
      assertEq({ -1.0f, 0.5f }, machine.getAbsoluteAxis2D(keys.move));
    }

    TEST_METHOD(LoopAxisEvent) {
      using namespace Input;
      InputMapper mapper;
      const KeyMapID axis = 0;
      const KeyMapID init = 1;
      const PlatformInputID increase = 0;
      const PlatformInputID decrease = 1;
      const EventID axisEvent = 1;
      mapper.addKeyAs1DRelativeMapping(increase, axis, 1.0f);
      mapper.addKeyAs1DRelativeMapping(decrease, axis, -1.0f);
      StateMachineBuilder builder;
      const NodeIndex root = builder.addNode(NodeBuilder{});
      const NodeIndex axisNode = builder.addNode(NodeBuilder{}.axis1D().emitEvent(axisEvent));
      const NodeIndex repeat = builder.addNode(NodeBuilder{});

      builder.addEdge(EdgeBuilder{}
        .from(StateMachine::ROOT_NODE)
        .to(root)
        .keyDown(init)
      );
      builder.addEdge(EdgeBuilder{}
        .from(root)
        .to(axisNode)
        .anyDelta1D(axis)
        //Consume prevents it from doing an immediate repeat, causing two events on button down
        .consumeEvent()
      );
      //TODO: this edge order is unintuitive, it's backwards because builder adds to front of list
      //Repeat event while axis is nonzero
      builder.addEdge(EdgeBuilder{}
        .from(axisNode)
        .to(repeat)
        .unconditional()
      );
      builder.addEdge(EdgeBuilder{}
        .from(repeat)
        .to(axisNode)
        .unconditional()
        .consumeEvent()
      );

      //Exit when axis goes back to zero
      builder.addEdge(EdgeBuilder{}
        .from(axisNode)
        .to(root)
        .absolute1D(axis, 0.0f, 0.0f)
        .consumeEvent()
      );

      StateMachine machine{ std::move(builder), std::move(mapper) };
      machine.traverse(machine.getMapper().onPassthroughKeyDown(init));
      const InputMapper& m = machine.getMapper();
      const auto& events = machine.readEvents();

      auto assertAxisEventWithValue = [&events, axisEvent, &machine](float axisValue) {
        Assert::AreEqual(size_t(1), events.size());
        const Event& e = events[0];
        Assert::AreEqual(axisEvent, e.id);
        Assert::AreEqual(axisValue, machine.getAbsoluteAxis1D(e.toNode), 0.001f);
      };

      //Event should trigger on key down
      machine.traverse(m.onKeyDown(increase));
      assertAxisEventWithValue(1.0f);
      machine.clearEvents();

      //Event should repeat while key stays down. Do it twice to make sure it isn't a fluke
      for(int i = 0; i < 2; ++i) {
        machine.traverse(m.onTick(1));
        assertAxisEventWithValue(1.0f);
        machine.clearEvents();
      }

      //Event should stop when key is released
      machine.traverse(m.onKeyUp(decrease));
      Assert::IsTrue(events.empty());
      Assert::AreEqual(0.0f, machine.getAbsoluteAxis1D(axisNode), 0.01f);
    }
  };
}