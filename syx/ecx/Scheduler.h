#pragma once

#include "JobGraph.h"

namespace ecx {
  //Possible implementation for TaskContainer and JobContainer
  //Lock-free queue would be better but I don't have one at the moment
  template<class T>
  class LockQueue {
  public:
    std::optional<T> pop() {
      std::scoped_lock<std::mutex> lock(mMutex);
      if(mQueue.size()) {
        auto result = std::make_optional(std::move(mQueue.front()));
        mQueue.pop();
        return result;
      }
      return {};
    }

    bool empty() const {
      std::scoped_lock<std::mutex> lock(mMutex);
      return mQueue.empty();
    }

    void push_back(T value) {
      std::scoped_lock<std::mutex> lock(mMutex);
      mQueue.push(std::move(value));
    }

  private:
    mutable std::mutex mMutex;
    std::queue<T> mQueue;
  };

  template<class SchedulerT>
  class SchedulerExecutor;

  struct SchedulerConfig {
    size_t mNumThreads = 10;
  };

  //A scheduler to allow threaded execution of an ECS system job graph. During execution, the systems can use
  //SchedulerExecutor to enqueue immediate work for within that system, like parallel for
  //Due to this constrainted usage, no scoped handle tracking is exposed to reduce overhead
  //Containers are threadsafe queues, TaskContainer<std::function<void()>> JobContainer<std::shared_ptr<JobInfo<EntityT>>>
  template<class EntityT, template<class> class QueueT>
  class Scheduler {
  public:
    using SelfT = Scheduler<EntityT, QueueT>;
    using TaskContainer = QueueT<std::function<void()>>;
    using JobContainer = QueueT<std::shared_ptr<JobInfo<EntityT>>>;

    struct WorkerContext {
      EntityRegistry<EntityT>** mRegistry = nullptr;
      TaskContainer* mTasks = nullptr;
      JobContainer* mJobs = nullptr;
      std::atomic_uint32_t* mWorkersActive = nullptr;
      std::condition_variable* mWorkerCV = nullptr;
      std::atomic_bool* mIsRunning = nullptr;
      std::mutex* mMutex = nullptr;
      std::atomic_bool* mIsSyncing = nullptr;
    };

    Scheduler(const SchedulerConfig& config)
      : mJobContainer(std::make_unique<JobContainer>())
      , mTasks(std::make_unique<TaskContainer>())
      , mConfig(config) {

      WorkerContext context(createContext());
      mThreads.reserve(mConfig.mNumThreads);
      for(size_t i = 0; i < mConfig.mNumThreads; ++i) {
        mThreads.push_back(std::thread([context]() mutable {
          workerLoop(context);
        }));
      }
    }

    ~Scheduler() {
      {
        //Grab the lock so nothing goes to sleep between the running change and the notify
        std::scoped_lock<std::mutex> lock(mMutex);
        //Signify that all threads should stop
        mIsRunning = false;
        //Wake all threads. They will either wake now, be waiting on the mutex to sleep, or
        //be in the middle of execution. In all cases they will check mIsRunning before trying to sleep
        //and will exit
        mWorkerCV.notify_all();
      }
      for(auto&& thread : mThreads) {
        thread.join();
      }
    }

    //Execute the job graph. This call returns after all jobs are complete, while this thread assists execution
    //Do not destroy the scheduler while this is running
    void execute(EntityRegistry<EntityT>& registry, JobInfo<EntityT>& jobGraph) {
      //Populate temporary registry pointer for threads to use
      mRegistry = &registry;

      //Reset dependency count trackers
      JobGraph::resetDependencies(jobGraph);
      //Run the root node which will populate the initial tasks
      JobGraph::runSystems(registry, jobGraph, *mJobContainer);
      //Wake all threads to pick up the work that was just created
      mWorkerCV.notify_all();

      //Make this thread work until all work is complete
      WorkerContext context(createContext());
      syncLoop(context);

      //Clear registry pointer. At this point all worker threads are asleep
      mRegistry = nullptr;
    }

    //Queue a piece of work to get picked up potentially immediately
    //Not guaranteed to finish work before function exits, for such a guarantee, use SchedulerExecutor::sync
    void queueImmediate(const SchedulerExecutor<SelfT>&, std::function<void()> work) {
      mTasks->push_back(std::move(work));
      //Try to wake up a worker for it, could be redundant based on timing
      mWorkerCV.notify_one();
    }

    //Used for SchedulerExecutor::sync to assist work until all of its tasks are done as tracked by atomic
    void syncImmediateTasks(const SchedulerExecutor<SelfT>&, std::atomic_uint32_t& remainingWork) {
      //Recruit all possible workers
      mWorkerCV.notify_all();
      while(remainingWork.load()) {
        //Help finish the work on this thread, focused only on mTasks, not mJobs
        if(auto immediateWork = mTasks->pop()) {
          (*immediateWork)();
        }
        //In this case no work is left in the queue but some thread is still working on the task
        else {
          //Acquire the lock for the condition variable
          std::unique_lock<std::mutex> lock(mMutex);
          //See if the work finished while acquiring the lock
          if(!remainingWork.load()) {
            return;
          }
          //Wait for work to complete
          mWorkerCV.wait(lock);
        }
      }
    }

    //Try a single iteration of work
    //Returns true if there was work to be done
    static bool doWork(WorkerContext& context) {
      if(auto immediateWork = context.mTasks->pop()) {
        (*immediateWork)();
      }
      else if(auto job = context.mJobs->pop()) {
        JobGraph::runSystems(**context.mRegistry, **job, *context.mJobs);
      }
      else {
        return false;
      }
      return true;
    }

    //Used for the calling thread of `execute` to assist work until it is all done
    //Unlike background threads, this should exit when all work is done instead of going to sleep
    static void syncLoop(WorkerContext& context) {
      auto isDone = [&context] {
        //Done if jobs are complete and all workers went to sleep. Checking tasks is redundant because it would only be filled while workers are active
        return context.mJobs->empty() && !context.mWorkersActive->load();
      };
      while(true) {
        while(doWork(context)) {
          if(!isDone()) {
            //Some kind of work happened, notify all threads in case new work is unblocked
            //Mutex is needed to make sure notification doesn't happen between a thread's work check and going to sleep
            std::scoped_lock<std::mutex> lock(*context.mMutex);
            context.mWorkerCV->notify_all();
          }
        }
        //If all work is complete, acquire the lock to check again and make sure
        if(isDone()) {
          return;
        }

        std::unique_lock<std::mutex> lock(*context.mMutex);
        //If work came in while acquiring the lock, keep cycling
        if(isDone()) {
          return;
        }
        //Wait for completion of some task which will wake this back up
        context.mIsSyncing->store(true, std::memory_order_release);
        context.mWorkerCV->wait(lock);
        //Make sure flag is reset when this thread is awake again. This is redundant in some cases
        context.mIsSyncing->store(false, std::memory_order_relaxed);
      }
    }

    //Work loop for background threads. They do work if it's available, and when none is left go to sleep,
    //to be awakened when more work becomes available
    static void workerLoop(WorkerContext& context) {
      ++(*context.mWorkersActive);
      while(*context.mIsRunning) {
        //Do all possible work
        while(doWork(context)) {}
        //All work is done, get the lock to go to sleep
        std::unique_lock<std::mutex> lock(*context.mMutex);
        //See if work appeared while acquiring the lock
        if(!context.mTasks->empty() || !context.mJobs->empty() || !context.mIsRunning->load()) {
          //Some kind of work happened, notify all threads in case new work is unblocked
          //Mutex is needed to make sure notification doesn't happen between a thread's work check and going to sleep
          context.mWorkerCV->notify_all();
          continue;
        }
        if(context.mWorkersActive->fetch_sub(1, std::memory_order_relaxed) == 1) {
          //If this is the last worker and all work is done, wake the sync thread if it's waiting
          if(*context.mIsSyncing) {
            context.mIsSyncing->store(false, std::memory_order_release);
            //This wastefully wakes all threads instead of only the sync thread, but is simpler than separate CVs
            context.mWorkerCV->notify_all();
          }
        }
        //No work, go to sleep
        context.mWorkerCV->wait(lock);
        ++(*context.mWorkersActive);
      }
      --(*context.mWorkersActive);
    }

  private:
    WorkerContext createContext() {
      WorkerContext result;
      result.mRegistry = &mRegistry;
      result.mTasks = mTasks.get();
      result.mJobs = mJobContainer.get();
      result.mWorkersActive = &mWorkersActive;
      result.mWorkerCV = &mWorkerCV;
      result.mIsRunning = &mIsRunning;
      result.mMutex = &mMutex;
      result.mIsSyncing = &mIsSyncing;
      return result;
    }

    EntityRegistry<EntityT>* mRegistry = nullptr;
    std::unique_ptr<TaskContainer> mTasks;
    std::unique_ptr<JobContainer> mJobContainer;
    std::atomic_uint32_t mWorkersActive = 0;
    std::atomic_bool mIsRunning = true;
    std::atomic_bool mIsSyncing = false;
    std::condition_variable mWorkerCV;
    std::mutex mMutex;
    std::vector<std::thread> mThreads;
    const SchedulerConfig mConfig;
  };

  //Execution context for immediate tasks. Caller is responsible for ensuring this outlives
  //the tasks it queues.
  template<class SchedulerT>
  class SchedulerExecutor {
  public:
    SchedulerExecutor(SchedulerT& scheduler)
      : mScheduler(scheduler) {
    }

    //Queue a task to be executed as soon as possible, order not guaranteed
    void queueTask(std::function<void()> task) {
      ++mOutstandingTasks;
      mScheduler.queueImmediate(*this, [t(std::move(task)), this] {
        t();
      --mOutstandingTasks;
      });
    }

    //Block until all outstanding tasks are done
    void sync() {
      mScheduler.syncImmediateTasks(*this, mOutstandingTasks);
    }

  private:
    std::atomic_uint32_t mOutstandingTasks = 0;
    SchedulerT& mScheduler;
  };

  template<class EntityT>
  using DefaultSchedulerT = Scheduler<EntityT, LockQueue>;

  namespace create {
    template<class SchedulerT>
    auto schedulerExecutor(SchedulerT& scheduler) {
      return SchedulerExecutor<SchedulerT>(scheduler);
    }
  };
}