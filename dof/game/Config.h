#pragma once

#include "PhysicsConfig.h"
#include "Table.h"

struct DebugConfig {
};

struct GameConfig {
  float playerSpeed = 0.05f;
  float playerMaxStoppingForce = 0.05f;

  size_t explodeLifetime = 5;
  float explodeStrength = 0.05f;

  float cameraZoomSpeed = 0.3f;

  float boundarySpringConstant = 0.01f;

  float fragmentGoalDistance = 0.5f;

  size_t fragmentRows = 10;
  size_t fragmentColumns = 9;
};

struct GraphicsConfig {

};

using ConfigTable = Table<
  SharedRow<DebugConfig>,
  SharedRow<PhysicsConfig>,
  SharedRow<GameConfig>,
  SharedRow<GraphicsConfig>
>;