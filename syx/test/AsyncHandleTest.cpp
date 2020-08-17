#include "Precompile.h"
#include "CppUnitTest.h"

#include "threading/AsyncHandle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ThreadingTests {
  TEST_CLASS(AsyncHandleTests) {
  public:
    TEST_METHOD(AsyncResult_CreateValue_HasValue) {
      const std::string value("value");

      auto asyncResult = Async::createResult(value);

      Assert::AreEqual(*asyncResult, value, L"Value should match", LINE_INFO());
    }

    //Just make sure the syntax works
    TEST_METHOD(AsyncResult_CreateVoid_IsEmpty) {
      auto asyncResult = Async::createResult();
      *asyncResult;
    }

    TEST_METHOD(AsyncHandle_CreateVoid_IsPending) {
      Assert::IsTrue(Async::createAsyncHandle<void>()->getStatus() == AsyncStatus::Pending, L"New task should be pending", LINE_INFO());
    }

    TEST_METHOD(AsyncHandle_CompleteVoid_IsComplete) {
      auto handle = Async::createAsyncHandle<void>();

      Async::setComplete(*handle);

      Assert::IsTrue(handle->getStatus() == AsyncStatus::Complete, L"Completed task should have completed state", LINE_INFO());
    }

    TEST_METHOD(AsyncHandle_CompleteInt_IsComplete) {
      auto handle = Async::createAsyncHandle<int>();

      Async::setComplete(*handle, 10);

      Assert::IsTrue(handle->getStatus() == AsyncStatus::Complete, L"Completed tasks should have completed state", LINE_INFO());
      Assert::AreEqual(*handle->getResult(), 10, L"Value should match what was completed", LINE_INFO());
    }

    TEST_METHOD(AsyncHandle_TakeResult_MatchesGet) {
      auto handle = Async::createAsyncHandle<std::string>();

      Async::setComplete(*handle, std::string("done"));

      const std::string get = *handle->getResult();
      const std::string take = *handle->takeResult();
      Assert::AreEqual(get, take, L"Get value should match take value", LINE_INFO());
    }

    TEST_METHOD(AsyncHandle_AddCallback_IsCalledOnCompletion) {
      auto handle = Async::createAsyncHandle<std::string>();

      bool wasCalled = false;
      const std::string value = "value";
      handle->then([&wasCalled, &value](IAsyncHandle<std::string>& handle) {
        Assert::AreEqual(*handle.getResult(), value, L"Should have completed value", LINE_INFO());
        wasCalled = true;
      });

      Assert::IsFalse(wasCalled, L"Callback shouldn't be called before task is completed", LINE_INFO());

      Async::setComplete(*handle, value);

      Assert::IsTrue(wasCalled, L"Callback should be called once task is completed", LINE_INFO());
    }

    TEST_METHOD(AsyncHandle_MultithreadedCallbacks_AllComplete) {
      auto handle = Async::createAsyncHandle<bool>();
      const int numCallbacks = 10000;
      int numComplete = 0;
      std::atomic_bool threadStarted = false;

      std::thread thread([&handle, &numComplete, &threadStarted] {
        for(int i = 0; i < 10000; ++i) {
          handle->then([&numComplete](IAsyncHandle<bool>& result) {
            Assert::IsTrue(*result.getResult(), L"Value should match completion value", LINE_INFO());
            ++numComplete;
          });
          threadStarted = true;
        }
      });

      while(!threadStarted) {
        std::this_thread::yield();
      }

      Async::setComplete(*handle, true);
      thread.join();

      Assert::AreEqual(numCallbacks, numComplete, L"All callbacks should have been triggered", LINE_INFO());
    }

    TEST_METHOD(CompleteHandle_CreateVoid_IsComplete) {
      Assert::IsTrue(Async::createCompleteHandle()->getStatus() == AsyncStatus::Complete, L"Complete tasks should have completed state", LINE_INFO());
    }

    TEST_METHOD(AsyncHandle_ThenValues_CallbackAfterTask) {
      auto first = Async::createAsyncHandle<int>();

      bool wasCalled = false;
      auto second = Async::thenResult(*first, [&wasCalled](IAsyncHandle<int>& prev) {
        Assert::AreEqual(*prev.getResult(), 10, L"First value should match completion", LINE_INFO());
        wasCalled = true;
        return Async::createResult(std::string("complete"));
      });

      Assert::IsFalse(wasCalled, L"Callbacks shouldn't trigger before task completes", LINE_INFO());

      Async::setComplete(*first, 10);

      Assert::IsTrue(wasCalled, L"Callback should be triggered after completion", LINE_INFO());
      Assert::IsTrue(second->getStatus() == AsyncStatus::Complete, L"Then task should be completed", LINE_INFO());
      Assert::AreEqual(*second->getResult(), std::string("complete"), L"Wrapped value should match", LINE_INFO());
    }

    TEST_METHOD(AsyncHandle_ThenValuesToVoid_Compiles) {
      auto first = Async::createAsyncHandle<int>();
      auto second = Async::thenResult(*first, [](IAsyncHandle<int>&) {
        return Async::createResult();
      });
    }

    TEST_METHOD(AsyncHandle_ThenAsyncValues_CallbacksAreCalled) {
      auto original = Async::createAsyncHandle<int>();

      auto innerTask = Async::createAsyncHandle<std::string>();
      bool allChained = false;
      auto wrapper = Async::thenHandle(*original, [innerTask](IAsyncHandle<int>&) {
        return innerTask;
      });

      wrapper->then([&allChained](IAsyncHandle<std::string>& chained) {
        Assert::AreEqual(*chained.getResult(), std::string("complete"), L"Value should be passed through to wrapper", LINE_INFO());
        allChained = true;
      });

      Async::setComplete(*original, 10);
      Async::setComplete(*innerTask, std::string("complete"));

      Assert::IsTrue(allChained, L"All callbacks should be triggered", LINE_INFO());
    }

    TEST_METHOD(AsyncHandle_THenAsyncVoid_Compiles) {
      auto original = Async::createAsyncHandle<int>();
      auto innerTask = Async::createAsyncHandle<void>();
      Async::thenHandle(*original, [innerTask](IAsyncHandle<int>&) {
        return innerTask;
      });
      Async::setComplete(*original, 10);
      Async::setComplete(*innerTask);
    }

    TEST_METHOD(CompleteHandle_CreateInt_IsComplete) {
      const int value = 10;

      auto handle = Async::createCompleteHandle(value);

      Assert::IsTrue(handle->getStatus() == AsyncStatus::Complete, L"Completed tasks should be complete", LINE_INFO());
      Assert::AreEqual(*handle->getResult(), value, L"Completed value should match", LINE_INFO());
    }

    TEST_METHOD(CompleteHandle_AddCallback_IsCalled) {
      auto handle = Async::createCompleteHandle(10);

      bool wasCalled = false;
      handle->then([&wasCalled](IAsyncHandle<int>& handle) {
        Assert::IsTrue(*handle.getResult() == 10, L"Handle should have completed value", LINE_INFO());
        wasCalled = true;
      });

      Assert::IsTrue(wasCalled, L"Callback should be called when it is added for compelted results", LINE_INFO());
    }
  };
}