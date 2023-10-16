#include "Precompile.h"
#include "GameScheduler.h"

#include "AppBuilder.h"
#include "Scheduler.h"
#include "ThreadLocals.h"

namespace GameScheduler {
  constexpr size_t MAIN_THREAD = 0;

  struct ConversionTask {
    AppTaskNode* src{};
    TaskNode* dst{};
  };

  void setConfigurableTask(enki::ITaskSet& task, AppTask& info) {
    if(info.config) {
      //Raw capture means it's the responsibility of the app to ensure it won't destroy tasks while they're running,
      //which should be reasonable
      info.config->setSize = [&task](const AppTaskSize& desiredSize) {
        task.m_MinRange = static_cast<uint32_t>(desiredSize.batchSize);
        task.m_SetSize = static_cast<uint32_t>(desiredSize.workItemCount);
      };
    }
  }

  struct TaskAdapter : enki::ITaskSet {
    TaskAdapter(AppTask&& t, ThreadLocals& tl)
      : task{ std::move(t) }
      , tls{ tl }{
      setConfigurableTask(*this, task);
    }

    void ExecuteRange(enki::TaskSetPartition range, uint32_t thread) override {
      AppTaskArgs args;
      args.begin = range.start;
      args.end = range.end;
      ThreadLocalData data = tls.get(thread);
      args.threadLocal = &data;

      task.callback(args);
    }

    AppTask task;
    ThreadLocals& tls;
  };

  struct PinnedTaskAdapter : enki::IPinnedTask {
    PinnedTaskAdapter(AppTask&& t, ThreadLocals& tl)
      : enki::IPinnedTask(MAIN_THREAD)
      , task{ std::move(t) }
      , tls{ tl } {
    }

    void Execute() override {
      //TODO: should these still have access to some form of thread local data?
      AppTaskArgs args;

      task.callback(args);
    }

    AppTask task;
    ThreadLocals& tls;
  };

  struct PopulateTask {
    void operator()(AppTaskPinning::None) {
      task.dst->mTask.mTask = std::make_unique<TaskAdapter>(std::move(task.src->task), tls);
    }

    void operator()(AppTaskPinning::MainThread) {
      task.dst->mTask.mTask = std::make_unique<PinnedTaskAdapter>(std::move(task.src->task), tls);
    }

    void operator()(AppTaskPinning::Synchronous) {
      //Synchronous behavior is addressed by GmaeBuilder.cpp
      task.dst->mTask.mTask = std::make_unique<TaskAdapter>(std::move(task.src->task), tls);
    }

    ConversionTask& task;
    ThreadLocals& tls;
  };

  TaskRange buildTasks(std::shared_ptr<AppTaskNode> root, ThreadLocals& tls) {
    std::deque<ConversionTask> todo;
    auto result = std::make_shared<TaskNode>();
    todo.push_back({ root.get(), result.get() });
    while(!todo.empty()) {
      ConversionTask current = todo.front();
      todo.pop_front();

      //Fill in the task callback for this one
      std::visit(PopulateTask{ current, tls }, current.src->task.pinning);

      //Create empty children and add them to the todo list
      current.dst->mChildren.resize(current.src->children.size());
      for(size_t i = 0; i < current.src->children.size(); ++i) {
        current.dst->mChildren[i] = std::make_shared<TaskNode>();
        todo.push_back({ current.src->children[i].get(), current.dst->mChildren[i].get() });
      }
    }

    return TaskBuilder::buildDependencies(result);
  }

  std::vector<SyncWorkItem> buildSync(std::shared_ptr<AppTaskNode> root) {
    std::deque<std::shared_ptr<AppTaskNode>> todo;
    std::vector<SyncWorkItem> result;
    todo.push_back(root);
    while(!todo.empty()) {
      auto current = todo.front();
      todo.pop_front();
      result.push_back({ [current] {
        AppTaskArgs args;
        current->task.callback(args);
      }});
      todo.insert(todo.end(), current->children.begin(), current->children.end());
    }
    return result;
  }
};