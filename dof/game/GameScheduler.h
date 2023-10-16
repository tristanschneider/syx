#pragma once

struct AppTaskNode;
struct TaskNode;
struct ThreadLocals;
struct TaskRange;

namespace GameScheduler {
  struct SyncWorkItem {
    std::function<void()> work;
  };

  TaskRange buildTasks(std::shared_ptr<AppTaskNode> root, ThreadLocals& tls);
  std::vector<SyncWorkItem> buildSync(std::shared_ptr<AppTaskNode> root);
};