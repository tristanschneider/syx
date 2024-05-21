#pragma once

struct AppTaskArgs;
struct AppTaskSize;

namespace Tasks {
  using TaskCallback = std::function<void(AppTaskArgs&)>;
  struct TaskHandle {
    void* data{};
  };
  struct AwaitOptions {
  };
  //A scheduler that is "local" to a task, allowing it to queue subtasks and await their completion
  //Tasks must be awaited before completion of the task that queued them: no fire and forget tasks
  struct ILocalScheduler {
    virtual ~ILocalScheduler() = default;

    virtual TaskHandle queueTask(TaskCallback&& task, const AppTaskSize& size) = 0;
    //Caller should only await each task exactly once
    virtual void awaitTasks(const TaskHandle* tasks, size_t count, const AwaitOptions& ops) = 0;
    virtual size_t getThreadCount() const = 0;
  };

  struct ILocalSchedulerFactory {
    virtual ~ILocalSchedulerFactory() = default;
    virtual std::unique_ptr<ILocalScheduler> create() = 0;
  };
}