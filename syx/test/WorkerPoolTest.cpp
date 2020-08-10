#include "Precompile.h"
#include "CppUnitTest.h"

#include "threading/FunctionTask.h"
#include "threading/SyncTask.h"
#include "threading/WorkerPool.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LuaTests {
  TEST_CLASS(WorkerPoolTests) {
  public:
    void _awaitWithTimeout(const std::function<bool()> condition, std::chrono::milliseconds timeout) {
      std::chrono::high_resolution_clock clock;
      auto startTime = clock.now();
      while(!condition() && clock.now() - startTime < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      Assert::IsTrue(condition(), L"Timed out waiting for condition", LINE_INFO());
    }

    void _awaitAsyncCompletion(const Task* task) {
      //If task is provided, wait until it's done or time out
      if(task) {
        _awaitWithTimeout([task]() { return task->getState() == TaskState::Done; }, std::chrono::milliseconds(100));
      }
      //If task isn't provided, we don't know what to wait for, so wait an arbitrary amount that should be long enough
      else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    TEST_METHOD(WorkerPool_QueueTask_TaskExecutes) {
      WorkerPool pool(4);
      auto task = std::make_shared<FunctionTask>([] {});
      pool.queueTask(task);

      _awaitAsyncCompletion(task.get());
    }

    TEST_METHOD(WorkerPool_QueueTaskWithDependency_ExecutionIsInOrder) {
      WorkerPool pool(4);
      auto first = std::make_shared<FunctionTask>([this] {
        //Delay to give the second task a chance to start if dependencies are broken
        _awaitAsyncCompletion(nullptr);
      });
      auto second = std::make_shared<FunctionTask>([first] {
        Assert::IsTrue(first->getState() == TaskState::Done, L"Dependency should be complete before dependent starts");
      });
      first->then(second);
      pool.queueTask(first);
      pool.queueTask(second);
    }

    TEST_METHOD(WorkerPool_QueueTaskWithDependencies_ExecutionIsInOrder) {
      WorkerPool pool(4);
      std::vector<std::shared_ptr<FunctionTask>> dependencies;
      constexpr int dependencyCount = 100;

      auto dependent = std::make_shared<FunctionTask>([&dependencies, dependencyCount] {
        Assert::IsTrue(dependencies.size() == static_cast<size_t>(dependencyCount), L"All dependencies should have been created before the dependent executes", LINE_INFO());
        Assert::IsTrue(std::all_of(dependencies.begin(), dependencies.end(), [](const auto& dep) { return dep->getState() == TaskState::Done; }), L"All dependencies should be complete before dependent executes", LINE_INFO());
      });

      for(int i = 0; i < dependencyCount; ++i) {
        auto task = std::make_shared<FunctionTask>([this] {
          //Delay to give the second task a chance to start if dependencies are broken
          _awaitAsyncCompletion(nullptr);
        });
        task->then(dependent);
        pool.queueTask(task);
        dependencies.push_back(task);
      }
      pool.queueTask(dependent);

      _awaitWithTimeout([dependent] { return dependent->getState() == TaskState::Done; }, std::chrono::seconds(1));
    }

    TEST_METHOD(WorkerPool_DepenencyCompletesBeforeQueue_TaskDoesntStartUntilQueue) {
      WorkerPool pool(4);
      auto dependent = std::make_shared<FunctionTask>([] {});
      auto dependency = std::make_shared<FunctionTask>([] {});
      dependency->then(dependent);

      pool.queueTask(dependency);

      //Coax out the issue by giving the dependency time to kick off if the logic is incorrect
      _awaitAsyncCompletion(nullptr);
      Assert::IsTrue(dependent->getState() != TaskState::Done, L"Task shouldn't start before it is queued", LINE_INFO());

      pool.queueTask(dependent);
      _awaitAsyncCompletion(dependent.get());
    }

    TEST_METHOD(WorkerPool_DependentAddedDuringRun_DependentRuns) {
      WorkerPool pool(4);
      bool taskRunning = false;
      auto dependency = std::make_shared<FunctionTask>([this, &taskRunning] {
        taskRunning = true;
        //Pause for a moment to ensure dependency logic below triggers during execution
        _awaitAsyncCompletion(nullptr);
        taskRunning = false;
      });
      auto dependent = std::make_shared<FunctionTask>([] {});

      //Queue the task and wait to ensure the following logic overlaps with its execution
      pool.queueTask(dependency);
      _awaitWithTimeout([&taskRunning] { return taskRunning; }, std::chrono::milliseconds(100));
      //Add dependent while task is running
      dependency->then(dependent);

      //Make sure dependency completes
      _awaitAsyncCompletion(dependency.get());
    }
  };
}