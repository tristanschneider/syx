#include "Precompile.h"
#include "GameScheduler.h"

#include "AppBuilder.h"
#include "Scheduler.h"
#include "ThreadLocals.h"

namespace GameScheduler {
  struct ConversionTask {
    AppTaskNode* src{};
    TaskNode* dst{};
  };

  struct TaskAdapter : enki::ITaskSet {
    TaskAdapter(AppTask&& t, ThreadLocals& tl)
      : task{ std::move(t) }
      , tls{ tl }{
    }

    void ExecuteRange(enki::TaskSetPartition range, uint32_t thread) override {
      if(task.config) {
        //TODO: how does this work? Probably interface that can hook into this ITaskSet configuration
      }

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

  void populateTask(ConversionTask& task, ThreadLocals& tls) {
    //TODO: option for pinned tasks
    task.dst->mTask.mTask = std::make_unique<TaskAdapter>(std::move(task.src->task), tls);
  }

  TaskRange buildTasks(std::shared_ptr<AppTaskNode> root, ThreadLocals& tls) {
    std::deque<ConversionTask> todo;
    auto result = std::make_shared<TaskNode>();
    todo.push_back({ root.get(), result.get() });
    while(!todo.empty()) {
      ConversionTask current = todo.front();
      todo.pop_front();

      //Fill in the task callback for this one
      populateTask(current, tls);

      //Create empty children and add them to the todo list
      current.dst->mChildren.resize(current.src->children.size());
      for(size_t i = 0; i < current.src->children.size(); ++i) {
        current.dst->mChildren[i] = std::make_shared<TaskNode>();
        todo.push_back({ current.src->children[i].get(), current.dst->mChildren[i].get() });
      }
    }

    return TaskBuilder::buildDependencies(result);
  }
};