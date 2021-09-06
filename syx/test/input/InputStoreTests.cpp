#include "Precompile.h"
#include "CppUnitTest.h"

#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/InputEvents.h"
#include "event/LifecycleEvents.h"
#include "input/InputStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace InputTests {
  TEST_CLASS(InputStoreTests) {
    template<class... Events>
    void enqueueTickWithEvents(EventBuffer& buffer, Events&&... events) {
      buffer.push(FrameStart());
      (buffer.push(events),...);
      buffer.push(FrameEnd());
    }

    TEST_METHOD(InputStore_NoState_DefaultResults) {
      auto store = std::make_shared<InputStore>();

      Assert::IsTrue(KeyState::Up == store->getKeyState("a"));
      Assert::IsTrue(KeyState::Up == store->getKeyState(Key::KeyA));
      Assert::IsFalse(store->getKeyDown(Key::KeyA));
      Assert::IsFalse(store->getKeyDownOrTriggered(Key::KeyA));
      Assert::IsTrue(store->getKeyUp(Key::KeyA));
      Assert::IsFalse(store->getKeyTriggered(Key::KeyA));
      Assert::IsTrue(KeyState::Up == store->getAsciiState('a'));
      Assert::IsTrue(Syx::Vec2(0) == store->getMousePos());
      Assert::IsTrue(Syx::Vec2(0) == store->getMouseDelta());
      Assert::AreEqual(0.f, store->getWheelDelta());
    }

    TEST_METHOD(InputStore_KeyTriggered_IsTriggered) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, KeyEvent(Key::KeyA, KeyState::Triggered));
      handler.handleEvents(buffer);

      Assert::IsTrue(KeyState::Triggered == store->getKeyState("a"));
      Assert::IsTrue(KeyState::Triggered == store->getKeyState(Key::KeyA));
      Assert::IsFalse(store->getKeyDown(Key::KeyA));
      Assert::IsTrue(store->getKeyDownOrTriggered(Key::KeyA));
      Assert::IsFalse(store->getKeyUp(Key::KeyA));
      Assert::IsTrue(store->getKeyTriggered(Key::KeyA));
      Assert::IsTrue(KeyState::Triggered == store->getAsciiState('a'));
    }

    TEST_METHOD(InputStore_KeyTriggeredAndWait_IsDown) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, KeyEvent(Key::KeyA, KeyState::Triggered));
      enqueueTickWithEvents(buffer);
      handler.handleEvents(buffer);

      Assert::IsTrue(KeyState::Down == store->getKeyState("a"));
      Assert::IsTrue(KeyState::Down == store->getKeyState(Key::KeyA));
      Assert::IsTrue(store->getKeyDown(Key::KeyA));
      Assert::IsTrue(store->getKeyDownOrTriggered(Key::KeyA));
      Assert::IsFalse(store->getKeyUp(Key::KeyA));
      Assert::IsFalse(store->getKeyTriggered(Key::KeyA));
      Assert::IsTrue(KeyState::Down == store->getAsciiState('a'));
    }

    TEST_METHOD(InputStore_KeyDownAndUp_IsReleased) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, KeyEvent(Key::KeyA, KeyState::Triggered), KeyEvent(Key::KeyA, KeyState::Released));
      handler.handleEvents(buffer);

      Assert::IsTrue(KeyState::Released == store->getKeyState("a"));
      Assert::IsTrue(KeyState::Released == store->getKeyState(Key::KeyA));
      Assert::IsFalse(store->getKeyDown(Key::KeyA));
      Assert::IsFalse(store->getKeyDownOrTriggered(Key::KeyA));
      Assert::IsFalse(store->getKeyUp(Key::KeyA));
      Assert::IsFalse(store->getKeyTriggered(Key::KeyA));
      Assert::IsTrue(store->getKeyReleased(Key::KeyA));
      Assert::IsTrue(KeyState::Released == store->getAsciiState('a'));
    }

    TEST_METHOD(InputStore_KeyDownUpAndWait_IsUp) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, KeyEvent(Key::KeyA, KeyState::Triggered), KeyEvent(Key::KeyA, KeyState::Released));
      enqueueTickWithEvents(buffer);
      handler.handleEvents(buffer);

      Assert::IsTrue(KeyState::Up == store->getKeyState("a"));
      Assert::IsTrue(KeyState::Up == store->getKeyState(Key::KeyA));
      Assert::IsFalse(store->getKeyDown(Key::KeyA));
      Assert::IsFalse(store->getKeyDownOrTriggered(Key::KeyA));
      Assert::IsTrue(store->getKeyUp(Key::KeyA));
      Assert::IsFalse(store->getKeyTriggered(Key::KeyA));
      Assert::IsTrue(KeyState::Up == store->getAsciiState('a'));
    }

    TEST_METHOD(InputStore_MouseKeyDown_IsDownWithPosition) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, MouseKeyEvent(Key::LeftMouse, KeyState::Triggered, Syx::Vec2(1, 2)));
      handler.handleEvents(buffer);

      Assert::IsTrue(KeyState::Triggered == store->getKeyState(Key::LeftMouse));
      Assert::IsFalse(store->getKeyDown(Key::LeftMouse));
      Assert::IsTrue(store->getKeyDownOrTriggered(Key::LeftMouse));
      Assert::IsFalse(store->getKeyUp(Key::LeftMouse));
      Assert::IsTrue(store->getKeyTriggered(Key::LeftMouse));
      Assert::IsTrue(store->getMousePos() == Syx::Vec2(1, 2));
      Assert::IsTrue(store->getMouseDelta() == Syx::Vec2(0));
    }

    TEST_METHOD(InputStore_MouseMove_PosAndDeltaSet) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, MouseMoveEvent(Syx::Vec2(1, 2), Syx::Vec2(3, 4)));
      handler.handleEvents(buffer);

      Assert::IsTrue(store->getMousePos() == Syx::Vec2(1, 2));
      Assert::IsTrue(store->getMouseDelta() == Syx::Vec2(3, 4));
    }

    TEST_METHOD(InputStore_MouseMoveAndWait_PosSetDeltaCleared) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, MouseMoveEvent(Syx::Vec2(1, 2), Syx::Vec2(3, 4)));
      enqueueTickWithEvents(buffer);
      handler.handleEvents(buffer);

      Assert::IsTrue(store->getMousePos() == Syx::Vec2(1, 2));
      Assert::IsTrue(store->getMouseDelta() == Syx::Vec2(0));
    }

    TEST_METHOD(InputStore_MouseWheel_WheelAmountSet) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, MouseWheelEvent(1.f));
      handler.handleEvents(buffer);

      Assert::AreEqual(store->getWheelDelta(), 1.f);
    }

    TEST_METHOD(InputStore_MouseWheelAndWait_WheelAmountCleared) {
      EventHandler handler;
      auto store = std::make_shared<InputStore>();
      store->init(handler);
      EventBuffer buffer;
      enqueueTickWithEvents(buffer, MouseWheelEvent(1.f));
      enqueueTickWithEvents(buffer);
      handler.handleEvents(buffer);

      Assert::AreEqual(store->getWheelDelta(), 0.f);
    }
  };
}