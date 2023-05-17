#pragma once

struct GameDB;
struct TaskRange;

namespace Player {
  //Modify thread locals
  //Read gameplay extracted values
  //Write PlayerInput
  TaskRange updateInput(GameDB db);
}