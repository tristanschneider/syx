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

  static std::shared_ptr<TaskNode> createEmpty() {
    return create([](...){});
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
};

struct TaskBuilder {
  static void _buildDependencies(std::shared_ptr<TaskNode> root) {
    std::unordered_set<TaskNode*> visited;
    std::deque<TaskNode*> toVisit;
    toVisit.push_back(root.get());

    if(root && !root->mTask) {
      root->mTask = { std::make_unique<enki::TaskSet>([](...){}) };
    }

    while(!toVisit.empty()) {
      TaskNode* current = toVisit.front();
      toVisit.pop_front();
      if(!visited.insert(current).second) {
        continue;
      }

      const size_t existingDeps = current->mDependencies.size();
      assert(current->mChildren.size() >= existingDeps && "Dependencies should not be removed");
      const size_t newDeps = current->mChildren.size() - existingDeps;
      current->mDependencies.resize(current->mChildren.size());

      for(size_t i = 0; i < current->mChildren.size(); ++i) {
        auto child = current->mChildren[i];
        if(!child->mTask) {
          child->mTask = { std::make_unique<enki::TaskSet>([](...){}) };
        }

        //Always recurse in case new dependencies are somewhere below. Very inefficient
        toVisit.push_back(child.get());
        if(i >= existingDeps) {
          auto dependency = std::make_unique<enki::Dependency>();
          dependency->SetDependency(current->mTask.get(), child->mTask.get());
          current->mDependencies[i] = std::move(dependency);
        }
      }
    }
  }

  //Add dependency to all leaf nodes
  static void _addSyncDependency(TaskNode& root, std::shared_ptr<TaskNode> toAdd) {
    std::deque<TaskNode*> toVisit;
    std::unordered_set<TaskNode*> visited;
    toVisit.push_back(&root);

    while(!toVisit.empty()) {
      TaskNode& current = *toVisit.front();
      toVisit.pop_front();
      if(!visited.insert(&current).second) {
        continue;
      }

      if(current.mChildren.empty()) {
        current.mChildren.push_back(toAdd);
      }
      //Traverse into children unless another recursion case already got there
      else if(current.mChildren.front() != toAdd) {
        for(std::shared_ptr<TaskNode>& child : current.mChildren) {
          toVisit.push_back(child.get());
        }
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

  //Brute force search for debugging
  static bool hasCycle(const TaskNode& node) {
    std::deque<const TaskNode*> toVisit;
    std::unordered_set<const TaskNode*> visited;
    std::deque<const TaskNode*> toSearch;
    std::unordered_set<const TaskNode*> searched;

    toVisit.push_back(&node);
    auto takeFront = [](auto& queue) {
      auto result = queue.front();
      queue.pop_front();
      return result;
    };
    auto pushChildren = [](auto& queue, const TaskNode* node) {
      for(const auto& child : node->mChildren) {
        queue.push_back(child.get());
      }
    };
    while(!toVisit.empty()) {
      const TaskNode* visitedNode = takeFront(toVisit);
      if(!visited.insert(visitedNode).second) {
        continue;
      }
      //Traverse all from here to see if any nodes lead back to this
      pushChildren(toSearch, visitedNode);

      while(!toSearch.empty()) {
        const TaskNode* current = takeFront(toSearch);
        if(!searched.insert(current).second) {
          continue;
        }
        if(current == visitedNode) {
          return true;
        }
        pushChildren(toSearch, current);
      }

      pushChildren(toVisit, visitedNode);
    }
    return false;
  }
};