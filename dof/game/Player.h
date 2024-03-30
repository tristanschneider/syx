#pragma once

#include "RuntimeDatabase.h"
#include "GameInput.h"

namespace Config {
  struct GameConfig;
}
namespace GameInput {
  struct PlayerInputRow;
}

class RuntimeDatabaseTaskBuilder;
class IAppBuilder;
struct PlayerInput;
struct TaskRange;

namespace Player {
  void init(IAppBuilder& builder);
  void initAbility(Config::GameConfig& config, QueryResultRow<GameInput::PlayerInputRow>& input);
  void updateInput(IAppBuilder& builder);
}