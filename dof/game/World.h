#pragma once

struct GameDB;
struct TaskRange;

namespace World {
  TaskRange enforceWorldBoundary(GameDB db);
}