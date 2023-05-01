#pragma once

struct DebugConfig;
struct ExternalDatabases;
struct PhysicsConfig;
struct GameConfig;
struct GraphicsConfig;
struct GameDB;
struct StableElementMappings;
struct StatEffectDBOwned;
struct ThreadLocalData;
struct ThreadLocals;
struct StableIDRow;

template<class Element>
struct BasicRow;


struct ConfigAdapter {
  DebugConfig* debug{};
  PhysicsConfig* physics{};
  GameConfig* game{};
  GraphicsConfig* graphics{};
};

namespace StatEffect {
  struct Owner;
  struct Lifetime;
  struct Global;
}

namespace PositionStatEffect {
  struct CommandRow;
}

namespace VelocityStatEffect {
  struct CommandRow;
};

namespace LambdaStatEffect {
  struct LambdaRow;
};

struct StatEffectBaseAdapter {
  StatEffect::Owner* owner{};
  StatEffect::Lifetime* lifetime{};
  StatEffect::Global* global{};
};

struct PositionStatEffectAdapter {
  StatEffectBaseAdapter base;
  PositionStatEffect::CommandRow* command{};
};

struct VelocityStatEffectAdapter {
  StatEffectBaseAdapter base;
  VelocityStatEffect::CommandRow* command{};
};

struct LambdaStatEffectAdapter {
  StatEffectBaseAdapter base;
  LambdaStatEffect::LambdaRow* command{};
};

struct TransformAdapter {
  BasicRow<float>* posX{};
  BasicRow<float>* posY{};
  BasicRow<float>* rotX{};
  BasicRow<float>* rotY{};
};

struct PhysicsObjectAdapter {
  BasicRow<float>* linVelX{};
  BasicRow<float>* linVelY{};
  BasicRow<float>* angVel{};
};

struct GameObjectAdapter {
  TransformAdapter transform;
  PhysicsObjectAdapter physics;
  StableIDRow* stable{};
};

struct TableAdapters {
  static ConfigAdapter getConfig(GameDB db);
  static StableElementMappings& getStableMappings(GameDB db);
  static ThreadLocals& getThreadLocals(GameDB db);
  static ThreadLocalData getThreadLocal(GameDB db, size_t thread);
  static ExternalDatabases& getExternalDBs(GameDB db);
  static StatEffectDBOwned& getStatEffects(GameDB db);

  static PositionStatEffectAdapter getPositionEffects(GameDB db, size_t thread);
  static VelocityStatEffectAdapter getVelocityEffects(GameDB db, size_t thread);
  static LambdaStatEffectAdapter getLambdaEffects(GameDB db, size_t thread);

  static GameObjectAdapter getGameObjects(GameDB db);
};