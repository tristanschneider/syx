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
  void init(RuntimeDatabaseTaskBuilder&& task);
  void initAbility(Config::GameConfig& config, QueryResultRow<Row<PlayerInput>>& input);
  void setupScene(IAppBuilder& builder);
  void updateInput(IAppBuilder& builder);
}