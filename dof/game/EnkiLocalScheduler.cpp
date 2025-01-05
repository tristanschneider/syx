#include "Precompile.h"
#include "EnkiLocalScheduler.h"

#include "generics/PagedVector.h"
#include "ILocalScheduler.h"
#include "Profile.h"
#include "Scheduler.h"
#include "AppBuilder.h"
#include "ThreadLocals.h"
#include "GameTaskArgs.h"

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

    void initTaskSet(enki::TaskSet& set, TaskCallback&& task, const AppTaskSize& size) {
      if(size.batchSize) {
        set.m_MinRange = static_cast<uint32_t>(size.batchSize);
        set.m_SetSize = static_cast<uint32_t>(size.workItemCount);
      }
      set.m_Function = [cb{std::move(task)}, this](enki::TaskSetPartition partition, uint32_t thread) {
        GameTaskArgs gta{ partition, args.getTLS ? args.getTLS(thread) : ThreadLocalData{}, thread };
        cb(gta);
      };
    }

    TaskHandle queueTask(TaskCallback&& task, const AppTaskSize& size) final {
      tasks.emplace_back();
      auto& t = tasks.back();
      initTaskSet(t, std::move(task), size);
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

    struct LongTask : ILongTask {
      bool isDone() const final {
        return task.GetIsComplete();
      }

      virtual const enki::ICompletable* getHandle() const final {
        return &task;
      }

      enki::TaskSet task;
    };

    void awaitTasks(const ILongTask* toAwait, size_t count, const AwaitOptions&) final {
      for(size_t i = 0; i < count; ++i) {
        if(const enki::ICompletable* completable = toAwait[i].getHandle()) {
          args.scheduler.mScheduler.WaitforTask(completable);
        }
      }
    }

    std::shared_ptr<ILongTask> queueLongTask(TaskCallback&& task, const AppTaskSize& size) final {
      auto result = std::make_shared<LongTask>();
      //Keep the enki object alive at least long enough to complete
      auto doTask = [cb{std::move(task)}, result](AppTaskArgs& a) mutable {
        cb(a);
        result.reset();
      };
      initTaskSet(result->task, std::move(doTask), size);
      args.scheduler.mScheduler.AddTaskSetToPipe(&result->task);
      return result;
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