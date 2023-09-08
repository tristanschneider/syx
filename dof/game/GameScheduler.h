#pragma once

struct AppTaskNode;
struct TaskNode;
struct ThreadLocals;
struct TaskRange;

namespace GameScheduler {
  TaskRange buildTasks(std::shared_ptr<AppTaskNode> root, ThreadLocals& tls);
};