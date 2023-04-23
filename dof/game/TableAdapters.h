#pragma once

struct DebugConfig;
struct PhysicsConfig;
struct GameConfig;
struct GraphicsConfig;
struct GameDB;

struct ConfigAdapter {
  DebugConfig* debug{};
  PhysicsConfig* physics{};
  GameConfig* game{};
  GraphicsConfig* graphics{};
};

struct TableAdapters {
  static ConfigAdapter getConfig(GameDB db);
};