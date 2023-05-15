#pragma once

struct GameDB;
struct TaskRange;

namespace Player {
  TaskRange updateInput(GameDB db);
}