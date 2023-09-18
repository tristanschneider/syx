#pragma once

#include "RuntimeDatabase.h"

namespace Config {
  struct GameConfig;
}

class IAppBuilder;
struct PlayerInput;
struct GameDB;
struct TaskRange;

namespace Player {
  void init(GameDB db);
  void initAbility(Config::GameConfig& config, QueryResultRow<Row<PlayerInput>>& input);
  void setupScene(IAppBuilder& builder);
  //Modify thread locals
  //Read gameplay extracted values
  //Write PlayerInput
  TaskRange updateInput(GameDB db);
}