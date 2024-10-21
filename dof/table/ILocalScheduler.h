#pragma once

struct AppTaskArgs;
struct AppTaskSize;

namespace Tasks {
  using TaskCallback = std::function<void(AppTaskArgs&)>;
  struct TaskHandle {
    explicit operator bool() const {
      return data != nullptr;
    }
    void* data{};
  };
  struct AwaitOptions {
  };
  struct LinkOptions {
  };
  struct ILongTask {
    virtual ~ILongTask() = default;
    virtual bool isDone() const = 0;
  };
  //A scheduler that is "local" to a task, allowing it to queue subtasks and await their completion
  //Tasks must be awaited before completion of the task that queued them: no fire and forget tasks, except for queueFreeTask
  struct ILocalScheduler {
    virtual ~ILocalScheduler() = default;

    virtual TaskHandle queueTask(TaskCallback&& task, const AppTaskSize& size) = 0;
    //Convenience to chain a linked list of tasks together for await calls
    //May use LinkOptions in the future to also specify dependencies
    virtual void linkTasks(TaskHandle from, TaskHandle to, const LinkOptions& ops) = 0;
    //Caller should only await each task exactly once
    virtual void awaitTasks(const TaskHandle* tasks, size_t count, const AwaitOptions& ops) = 0;
    virtual size_t getThreadCount() const = 0;

    //Queue a task that extends outside of this one
    virtual std::shared_ptr<ILongTask> queueLongTask(TaskCallback&& task, const AppTaskSize& size) = 0;
  };

  struct ILocalSchedulerFactory {
    virtual ~ILocalSchedulerFactory() = default;
    virtual std::unique_ptr<ILocalScheduler> create() = 0;
  };
}