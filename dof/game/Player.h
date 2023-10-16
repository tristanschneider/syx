#pragma once

#include "RuntimeDatabase.h"

namespace Config {
  struct GameConfig;
}

class RuntimeDatabaseTaskBuilder;
class IAppBuilder;
struct PlayerInput;
struct TaskRange;

namespace Player {
  void init(IAppBuilder& builder);
  void initAbility(Config::GameConfig& config, QueryResultRow<Row<PlayerInput>>& input);
  void setupScene(IAppBuilder& builder);
  void updateInput(IAppBuilder& builder);
}