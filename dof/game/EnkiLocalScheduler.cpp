#include "Precompile.h"
#include "EnkiLocalScheduler.h"

#include "generics/PagedVector.h"
#include "ILocalScheduler.h"
#include "Profile.h"
#include "Scheduler.h"
#include "AppBuilder.h"

namespace Tasks {
  struct SchedulerArgs {
    Scheduler& scheduler;
  };
  struct EnkiScheduler : ILocalScheduler {
    EnkiScheduler(SchedulerArgs& s)
      : args{ s }
    {
    }

    TaskHandle wrap(enki::TaskSet& task) const {
      return { &task };
    }

    enki::TaskSet& unwrap(const TaskHandle& handle) {
      return *static_cast<enki::TaskSet*>(handle.data);
    }

    TaskHandle queueTask(TaskCallback&& task, const AppTaskSize& size) final {
      tasks.emplace_back();
      auto& t = tasks.back();
      if(size.batchSize) {
        t.m_MinRange = static_cast<uint32_t>(size.batchSize);
        t.m_SetSize = static_cast<uint32_t>(size.workItemCount);
      }
      t.m_Function = [cb{std::move(task)}](enki::TaskSetPartition partition, uint32_t thread) {
        PROFILE_SCOPE("scheduler", "local");
        AppTaskArgs args;
        //TODO: populate other args as well
        args.begin = partition.start;
        args.end = partition.end;
        args.threadIndex = thread;
        cb(args);
      };
      ++tasksRemaining;
      args.scheduler.mScheduler.AddTaskSetToPipe(&t);
      return wrap(t);
    }

    void awaitTasks(const TaskHandle* toAwait, size_t count, const AwaitOptions&) final {
      assert(tasksRemaining >= count);
      for(size_t i = 0; i < count; ++i) {
        args.scheduler.mScheduler.WaitforTask(&unwrap(toAwait[i]));
      }
      //Keep count of remaining then clear the list when it hits zero
      //This is simpler than tracking holes and works fine for the intended case of firing off a bunch
      //of tasks and waiting for all of them
      tasksRemaining -= std::min(tasksRemaining, count);
      if(!tasksRemaining) {
        tasks.clear();
      }
    }

    size_t getThreadCount() const final {
      return args.scheduler.mScheduler.GetNumTaskThreads();
    }

    SchedulerArgs args;
    gnx::PagedVector<enki::TaskSet> tasks;
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

  std::unique_ptr<ILocalSchedulerFactory> createEnkiSchedulerFactory(Scheduler& scheduler) {
    return std::make_unique<EnkiSchedulerFactory>(SchedulerArgs{ scheduler });
  }
}