#pragma once

struct GameDB;
struct TaskRange;

namespace Player {
  void init(GameDB db);
  void initAbility(GameDB db);
  void setupScene(GameDB game);
  //Modify thread locals
  //Read gameplay extracted values
  //Write PlayerInput
  TaskRange updateInput(GameDB db);
}