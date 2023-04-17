#pragma once

#include "TaskScheduler.h"

#include <variant>

using OwnedDependency = std::unique_ptr<enki::Dependency>;

struct OwnedTask {
  enki::ICompletable* get() {
    return std::visit([](auto& task) -> enki::ICompletable* { return task.get(); }, mTask);
  }

  const enki::ICompletable* get() const {
    return std::visit([](const auto& task) -> const enki::ICompletable*{ return task.get(); }, mTask);
  }

  void addToPipe(enki::TaskScheduler& scheduler) {
    struct Adapter {
      void operator()(std::unique_ptr<enki::ITaskSet>& task) const {
        scheduler->AddTaskSetToPipe(task.get());
      }
      void operator()(std::unique_ptr<enki::IPinnedTask>& task) const {
        scheduler->AddPinnedTask(task.get());
      }
      enki::TaskScheduler* scheduler{};
    };
    std::visit(Adapter{ &scheduler }, mTask);
  }

  void setSize(uint32_t size, uint32_t minRange) {
    std::visit([=]([[maybe_unused]] auto& task) {
      if constexpr(std::is_same_v<enki::ITaskSet, std::decay_t<decltype(*task)>>) {
        task->m_SetSize = size;
        task->m_MinRange = minRange;
      }
    }, mTask);
  }

  operator bool() const {
    return get() != nullptr;
  }

  std::variant<std::unique_ptr<enki::ITaskSet>, std::unique_ptr<enki::IPinnedTask>> mTask;
};

struct TaskNode {
  static std::shared_ptr<TaskNode> create(enki::TaskSetFunction f) {
    assert(f);
    auto result = std::make_shared<TaskNode>();
    result->mTask.mTask = { std::make_unique<enki::TaskSet>(std::move(f)) };
    return result;
  }

  static std::shared_ptr<TaskNode> createMainThreadPinned(enki::PinnedTaskFunction f) {
    assert(f);
    auto result = std::make_shared<TaskNode>();
    result->mTask.mTask = { std::make_unique<enki::LambdaPinnedTask>(std::move(f)) };
    return result;
  }

  OwnedTask mTask;
  std::vector<std::shared_ptr<TaskNode>> mChildren;
  std::vector<std::unique_ptr<enki::Dependency>> mDependencies;
};

struct TaskRange {
  std::shared_ptr<TaskNode> mBegin, mEnd;
};

struct Scheduler {
  enki::TaskScheduler mScheduler;
  TaskRange mNodeRange;
};

struct TaskBuilder {
  static void _buildDependencies(std::shared_ptr<TaskNode> root) {
    const size_t existingDeps = root->mDependencies.size();
    assert(root->mChildren.size() >= existingDeps && "Dependencies should not be removed");
    const size_t newDeps = root->mChildren.size() - existingDeps;
    root->mDependencies.resize(root->mChildren.size());
    if(!root->mTask) {
      root->mTask = { std::make_unique<enki::TaskSet>([](...){}) };
    }
    for(size_t i = 0; i < root->mChildren.size(); ++i) {
      auto child = root->mChildren[i];
      //Always recurse in case new dependencies are somewhere below. Very inefficient
      _buildDependencies(child);
      if(i >= existingDeps) {
        auto dependency = std::make_unique<enki::Dependency>();
        dependency->SetDependency(root->mTask.get(), child->mTask.get());
        root->mDependencies[i] = std::move(dependency);
      }
    }
  }

  //Add dependency to all leaf nodes
  static void _addSyncDependency(TaskNode& current, std::shared_ptr<TaskNode> toAdd) {
    if(current.mChildren.empty()) {
      current.mChildren.push_back(toAdd);
    }
    //Traverse into children unless another recursion case already got there
    else if(current.mChildren.front() != toAdd) {
      for(std::shared_ptr<TaskNode>& child : current.mChildren) {
        _addSyncDependency(*child, toAdd);
      }
    }
  }

  static TaskRange addEndSync(std::shared_ptr<TaskNode> current) {
    auto end = std::make_shared<TaskNode>();
    _addSyncDependency(*current, end);
    return { current, end };
  }

  static TaskRange buildDependencies(std::shared_ptr<TaskNode> root) {
    auto finalNode = std::make_shared<TaskNode>();
    finalNode->mTask = { std::make_unique<enki::TaskSet>([](...){}) };
    _addSyncDependency(*root, finalNode);
    _buildDependencies(root);
    return { root, finalNode };
  }
};