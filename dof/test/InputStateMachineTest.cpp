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

  Keys createStateMachine(Input::StateMachine& machine);
};

namespace InputEvents {
  constexpr Input::EventID ACTION_1_BEGIN{ 0 };
  constexpr Input::EventID ACTION_1_TRIGGER{ 1 };
  constexpr Input::EventID ACTION_2_TRIGGER{ 2 };
  constexpr Input::EventID DEBUG_ZOOM{ 3 };
}

namespace InputMappings {
  void addAndExit(Input::StateMachine& machine, const Input::EdgeData& data) {
    machine.addEdge(data);
    machine.addEdge(Input::EdgeBuilder{}
      .from(data.to)
      .to(Input::StateMachine::ROOT_NODE)
      .unconditional()
    );
  }

  Keys createStateMachine(Input::StateMachine& machine) {
    //Having this as a root should make it possible to exit player control if desired by traversing out of the node
    //Kind of, any substates would need to finish
    auto playerRoot = machine.addNode({ Input::Node::Empty{} });
    auto move = machine.addNode({ Input::Node::Axis2D{} });
    auto zoom = machine.addNode({ Input::Node::Axis1D{} });
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
      .delta2D(InputMappings::MOVE)
      .forkState()
    );
    //Fork into zoom and exit
    addAndExit(machine, Input::EdgeBuilder{}
      .from(playerRoot)
      .to(zoom)
      .delta1D(InputMappings::DEBUG_ZOOM)
      .forkState()
    );
    //Press
    machine.addEdge(Input::EdgeBuilder{}
      .from(playerRoot)
      .to(action1Begin)
      .keyDown(InputMappings::ACTION_1)
      .forkState()
    );
    //TODO: hold doesn't really need its own state but maybe it's nice for clarity
    //Hold
    machine.addEdge(Input::EdgeBuilder{}
      .from(action1Begin)
      .to(action1Hold)
      .unconditional()
    );
    addAndExit(machine, Input::EdgeBuilder{}
      .from(action1Hold)
      .to(action1Release)
    );
    addAndExit(machine, Input::EdgeBuilder{}
      .from(playerRoot)
      .to(action2)
      .keyDown(InputMappings::ACTION_2)
      .forkState()
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
    TEST_METHOD(asdf) {
      Input::InputMapper mapper;
      Input::StateMachine machine;
      buildPlatformMappings(mapper);
      InputMappings::Keys keys = InputMappings::createStateMachine(machine);
      mapper.initializeStateMachine(machine);
      const std::vector<Input::Event>& events = machine.readEvents();

      //Root player node disabled so nothing should happen
      machine.traverse(mapper.onKeyDown(PlatformInput::A2));
      Assert::IsTrue(events.empty());

      //TODO: the input that doesn't use the mapper is awkward
      machine.traverse({ Input::Edge::KeyDown{ InputMappings::ENABLE_PLAYER_INPUT } });

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
      Assert::AreEqual(InputEvents::ACTION_2_TRIGGER, events[0].id);

      //Trigger Action 1 on and make sure it fires the event on release
      machine.traverse(mapper.onKeyDown(PlatformInput::A1));
      Assert::AreEqual(size_t(1), events.size());
      Assert::AreEqual(InputEvents::ACTION_1_BEGIN, events[0].id);
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
      machine.traverse(mapper.onAxis2DAbsolute(InputMappings::MOVE, { 0.5f, 0.5f }));
      assertEq({ 0.5f, 0.5f }, machine.getAbsoluteAxis2D(InputMappings::MOVE));
      machine.traverse(mapper.onAxis2DRelative(InputMappings::MOVE, { -0.5f, 0.0f }));
      assertEq({ 0.0f, 0.5f }, machine.getAbsoluteAxis2D(InputMappings::MOVE));

      //2D input through key mappings while thumbstick from above is still held
      machine.traverse(mapper.onKeyDown(PlatformInput::S));
      machine.traverse(mapper.onKeyDown(PlatformInput::A));
      assertEq({ -1, -0.5f }, machine.getAbsoluteAxis2D(InputMappings::MOVE));
      machine.traverse(mapper.onKeyUp(PlatformInput::S));
      assertEq({ -1.0f, 0.5f }, machine.getAbsoluteAxis2D(InputMappings::MOVE));
    }
  };
}