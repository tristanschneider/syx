#include "Precompile.h"
#include "EnkiLocalScheduler.h"

#include "generics/PagedVector.h"
#include "ILocalScheduler.h"
#include "Profile.h"
#include "Scheduler.h"
#include "AppBuilder.h"
#include "ThreadLocals.h"

namespace Tasks {
  struct SchedulerArgs {
    Scheduler& scheduler;
    GetThreadLocal getTLS;
  };
  struct LocalTask : enki::TaskSet {
    using enki::TaskSet::TaskSet;
    LocalTask* next{};
  };
  struct EnkiScheduler : ILocalScheduler {
    EnkiScheduler(SchedulerArgs& s)
      : args{ s }
    {
    }

    TaskHandle wrap(LocalTask& task) const {
      return { &task };
    }

    LocalTask& unwrap(const TaskHandle& handle) {
      return *static_cast<LocalTask*>(handle.data);
    }

    TaskHandle queueTask(TaskCallback&& task, const AppTaskSize& size) final {
      tasks.emplace_back();
      auto& t = tasks.back();
      if(size.batchSize) {
        t.m_MinRange = static_cast<uint32_t>(size.batchSize);
        t.m_SetSize = static_cast<uint32_t>(size.workItemCount);
      }
      t.m_Function = [cb{std::move(task)}, this](enki::TaskSetPartition partition, uint32_t thread) {
        AppTaskArgs taskArgs;
        taskArgs.begin = partition.start;
        taskArgs.end = partition.end;
        taskArgs.threadIndex = thread;
        ThreadLocalData tld;
        taskArgs.threadLocal = &tld;
        if(args.getTLS) {
          tld = args.getTLS(taskArgs.threadIndex);
          taskArgs.scheduler = tld.scheduler;
        }
        cb(taskArgs);
      };
      ++tasksRemaining;
      args.scheduler.mScheduler.AddTaskSetToPipe(&t);
      return wrap(t);
    }

    void linkTasks(TaskHandle from, TaskHandle to, const LinkOptions&) final {
      assert(unwrap(from).next == nullptr);
      unwrap(from).next = &unwrap(to);
    }

    void awaitTasks(const TaskHandle* toAwait, size_t count, const AwaitOptions&) final {
      assert(tasksRemaining >= count);
      size_t tasksFinished = 0;
      for(size_t i = 0; i < count; ++i) {
        LocalTask* current = &unwrap(toAwait[i]);
        while(current) {
          args.scheduler.mScheduler.WaitforTask(current);
          current = current->next;
          ++tasksFinished;
        }
      }
      //Keep count of remaining then clear the list when it hits zero
      //This is simpler than tracking holes and works fine for the intended case of firing off a bunch
      //of tasks and waiting for all of them
      tasksRemaining -= std::min(tasksRemaining, tasksFinished);
      if(!tasksRemaining) {
        tasks.clear();
      }
    }

    size_t getThreadCount() const final {
      return args.scheduler.mScheduler.GetNumTaskThreads();
    }

    SchedulerArgs args;
    gnx::PagedVector<LocalTask> tasks;
    size_t tasksRemaining{};
  };

  struct EnkiSchedulerFactory : ILocalSchedulerFactory {
    EnkiSchedulerFactory(SchedulerArgs a)
      : args{ a }
    {
    }

    std::unique_ptr<ILocalScheduler> create() final {
      return std::make_unique<EnkiScheduler>(args);
    }

    SchedulerArgs args;
  };

  std::unique_ptr<ILocalSchedulerFactory> createEnkiSchedulerFactory(Scheduler& scheduler, GetThreadLocal&& getThread) {
    return std::make_unique<EnkiSchedulerFactory>(SchedulerArgs{ scheduler, std::move(getThread) });
  }
}