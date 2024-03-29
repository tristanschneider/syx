#include "Precompile.h"
#include "CppUnitTest.h"

#include "event/Event.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/SpaceEvents.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EventTests {
  struct TestPlainEvent : public TypedEvent<TestPlainEvent> {
  };

  struct TestValueEvent : public TypedEvent<TestValueEvent> {
    std::string mValue;
    std::function<void()> mFunc;
  };

  struct TestResponseEvent : public TypedEvent<TestResponseEvent> {
    int value = 0;
  };

  struct TestRequestEvent : public RequestEvent<TestRequestEvent, TestResponseEvent> {
  };

  struct TestRequestPlainResponse : public RequestEvent<TestRequestPlainResponse, bool> {
  };

  struct EventTester {
    EventBuffer mWriteBuffer;
    EventBuffer mReadBufferA;
    EventBuffer mReadBufferB;
    EventHandler mHandlerA;
    EventHandler mHandlerB;

    //Simulates the way the engine dispatches events to systems
    void dispatchToReadBuffers() {
      mReadBufferA.clear();
      mWriteBuffer.appendTo(mReadBufferA);

      mReadBufferB.clear();
      mWriteBuffer.appendTo(mReadBufferB);

      mWriteBuffer.clear();
    }

    void dispatchAndProcessMessages() {
      dispatchToReadBuffers();
      processMessages();
    }

    void processMessages() {
      mHandlerA.handleEvents(mReadBufferA);
      mHandlerB.handleEvents(mReadBufferB);
    }
  };

  TEST_CLASS(EventBufferTests) {
    TEST_METHOD(EventBuffer_AddToBuffer_IsValid) {
      EventTester tester;

      std::string str = "this is a string for testing";
      TestValueEvent e;
      e.mValue = str;
      e.mFunc = []{};
      tester.mWriteBuffer.push(std::move(e));

      const TestValueEvent& event = static_cast<const TestValueEvent&>(*tester.mWriteBuffer.begin());
      Assert::IsTrue(event.mValue == str, L"Value should still match after being pushed", LINE_INFO());
      event.mFunc();
    }

    TEST_METHOD(EventBuffer_AppendToBuffer_IsValid) {
      EventTester tester;

      std::string str = "this is a string for testing";
      TestValueEvent e;
      e.mValue = str;
      e.mFunc = []{};
      tester.mWriteBuffer.push(std::move(e));

      tester.dispatchToReadBuffers();

      const TestValueEvent& event = static_cast<const TestValueEvent&>(*tester.mReadBufferA.begin());
      Assert::IsTrue(event.mValue == str, L"Value should still match after being pushed", LINE_INFO());
      event.mFunc();
    }


    TEST_METHOD(EventBuffer_RegisterHandler_HandlesEvent) {
      EventTester tester;

      bool handled = false;
      auto reg = tester.mHandlerA.registerEventListener([&handled](const TestPlainEvent&) {
        handled = true;
      });

      tester.mWriteBuffer.push(TestPlainEvent());
      tester.dispatchAndProcessMessages();

      Assert::IsTrue(handled, L"Processing events should have triggered registered handler", LINE_INFO());
    }

    TEST_METHOD(EventBuffer_RequestResponse_FinalCallbackTriggered) {
      EventTester tester;

      size_t handled = 0;
      TestRequestEvent request;
      request.then(typeId<EventTester, System>(), [&handled](const TestResponseEvent& res) {
        ++handled;
        Assert::IsTrue(res.value == 10, L"Value on response should have been preserved", LINE_INFO());
      });
      auto regA = tester.mHandlerA.registerEventListener(CallbackEvent::getHandler(typeId<EventTester, System>()));
      auto regB = tester.mHandlerB.registerEventListener([&tester](const TestRequestEvent& e) mutable {
        TestResponseEvent res;
        res.value = 10;
        e.respond(tester.mWriteBuffer, std::move(res));
      });

      tester.mWriteBuffer.push(std::move(request));
      //First call should dispatch request to B, B should enqueue response, but it shouldn't be handled yet
      tester.dispatchAndProcessMessages();
      Assert::IsTrue(!handled, L"Request should not be finished yet since the response should still be in flight", LINE_INFO());
      tester.dispatchAndProcessMessages();
      Assert::IsTrue(handled == 1, L"Request should have been handled exactly once", LINE_INFO());
    }

    TEST_METHOD(EventBuffer_RequestResponsePlainType_FinalCallbackTriggered) {
      EventTester tester;

      size_t handled = 0;
      TestRequestPlainResponse request;
      request.then(typeId<EventTester, System>(), [&handled](bool res) {
        ++handled;
        Assert::IsTrue(res, L"Value on response should have been preserved", LINE_INFO());
      });
      auto regA = tester.mHandlerA.registerEventListener(CallbackEvent::getHandler(typeId<EventTester, System>()));
      //Register this to make sure it doesn't trigger a double response
      auto regB = tester.mHandlerB.registerEventListener(CallbackEvent::getHandler(typeId<int, System>()));
      auto regC = tester.mHandlerB.registerEventListener([&tester](const TestRequestPlainResponse& e) mutable {
        e.respond(tester.mWriteBuffer, true);
      });

      tester.mWriteBuffer.push(std::move(request));
      //First call should dispatch request to B, B should enqueue response, but it shouldn't be handled yet
      tester.dispatchAndProcessMessages();
      Assert::IsTrue(!handled, L"Request should not be finished yet since the response should still be in flight", LINE_INFO());
      tester.dispatchAndProcessMessages();
      Assert::IsTrue(handled == 1, L"Request should have been handled exactly once", LINE_INFO());
    }

    TEST_METHOD(EventBuffer_invokeExpiredCallback_NotCalled) {
      EventTester tester;
      auto l = tester.mHandlerA.registerEventListener([](const TestPlainEvent&) {
        Assert::Fail(L"Expired listeners should not be invoked");
      });
      l.reset();

      tester.mWriteBuffer.push(TestPlainEvent());
      tester.dispatchAndProcessMessages();
    }

    TEST_METHOD(EventBuffer_InvokeExpiredListener_NotCalled) {
      struct TestListener : public EventListener {
        void onEvent(const Event&) override {
          Assert::Fail(L"Generic event listener should not have triggered for expired listener");
        }

        void onPlainEvent(const TestPlainEvent&) {
          Assert::Fail(L"Custom event listener should not be triggered for expired listeners");
        }
      };
      EventTester tester;
      auto listener = std::make_shared<TestListener>();
      tester.mHandlerA.registerEventListener(listener, &TestListener::onPlainEvent);

      listener.reset();
      tester.mWriteBuffer.push(TestPlainEvent());
      tester.dispatchAndProcessMessages();
    }
  };
}