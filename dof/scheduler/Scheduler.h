#pragma once

#include "TaskScheduler.h"

using OwnedTask = std::unique_ptr<enki::ITaskSet>;
using OwnedDependency = std::unique_ptr<enki::Dependency>;

struct TaskNode {
  static std::shared_ptr<TaskNode> create(enki::TaskSetFunction f) {
    assert(f);
    auto result = std::make_shared<TaskNode>();
    result->mTask = std::make_unique<enki::TaskSet>(std::move(f));
    return result;
  }

  OwnedTask mTask;
  std::vector<std::shared_ptr<TaskNode>> mChildren;
  std::vector<enki::Dependency> mDependencies;
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
    root->mDependencies.resize(root->mChildren.size());
    if(!root->mTask) {
      root->mTask = std::make_unique<enki::TaskSet>([](...){});
    }
    for(size_t i = 0; i < root->mChildren.size(); ++i) {
      //Only recurse if dependencies for that haven't been built yet
      //Would be false for diamond dependencies:
      //  A
      // / \
      //B   C
      // \ /
      //  D
      //D would be visited by B and then shouldn't recurse when visited again by C
      auto child = root->mChildren[i];
      if(child->mDependencies.empty()) {
        _buildDependencies(root->mChildren[i]);
      }
      root->mDependencies[i].SetDependency(root->mTask.get(), child->mTask.get());
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
    finalNode->mTask = std::make_unique<enki::TaskSet>([](...){});
    _addSyncDependency(*root, finalNode);
    _buildDependencies(root);
    return { root, finalNode };
  }
};