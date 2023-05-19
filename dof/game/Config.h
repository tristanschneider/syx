#pragma once

#include "curve/CurveDefinition.h"
#include "PhysicsConfig.h"
#include "Table.h"

struct DebugConfig {
};

struct PlayerConfig {
  CurveDefinition linearMoveCurve;
  CurveDefinition angularMoveCurve;
  CurveDefinition linearStoppingCurve;
};

struct PlayerAbilityConfig {
  size_t explodeLifetime = 5;
  float explodeStrength = 0.05f;
};

struct CameraConfig {
  float cameraZoomSpeed = 0.3f;
};

struct FragmentConfig {
  float fragmentGoalDistance = 0.5f;

  size_t fragmentRows = 10;
  size_t fragmentColumns = 9;
};

struct WorldConfig {
  float deltaTime = 1.0f/60.0f;
  float boundarySpringConstant = 0.01f;
};

struct GraphicsConfig {
};

struct GameConfig {
  PlayerConfig player;
  PlayerAbilityConfig ability;
  CameraConfig camera;
  FragmentConfig fragment;
  WorldConfig world;
};


using ConfigTable = Table<
  SharedRow<DebugConfig>,
  SharedRow<PhysicsConfig>,
  SharedRow<GameConfig>,
  SharedRow<GraphicsConfig>
>;