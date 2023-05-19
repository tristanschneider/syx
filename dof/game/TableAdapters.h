#pragma once

#include "TableOperations.h"

struct DebugConfig;
struct ExternalDatabases;
struct GameConfig;
struct GraphicsConfig;
struct GameDB;
struct PhysicsConfig;
struct StableElementMappings;
struct StableIDRow;
struct StatEffectDBOwned;
struct ThreadLocalData;
struct ThreadLocals;

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

namespace AreaForceStatEffect {
  struct PointX;
  struct PointY;
  struct Strength;
};

struct StatEffectBaseAdapter {
  StatEffect::Owner* owner{};
  StatEffect::Lifetime* lifetime{};
  StatEffect::Global* global{};
  StableTableModifierInstance modifier;
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

struct AreaForceStatEffectAdapter {
  StatEffectBaseAdapter base;
  AreaForceStatEffect::PointX* pointX{};
  AreaForceStatEffect::PointY* pointY{};
  AreaForceStatEffect::Strength* strength{};
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

  BasicRow<float>* linImpulseX{};
  BasicRow<float>* linImpulseY{};
  BasicRow<float>* angImpulse{};
};

struct GameObjectAdapter {
  TransformAdapter transform;
  PhysicsObjectAdapter physics;
  StableIDRow* stable{};
};

struct PlayerInput;
struct PlayerKeyboardInput;

struct PlayerAdapter {
  GameObjectAdapter object;
  BasicRow<PlayerInput>* input{};
  BasicRow<PlayerKeyboardInput>* keyboardInput{};
};

struct SceneState;
struct PhysicsTableIds;
struct FileSystem;
struct StableElementMappings;
struct ConstraintsTableMappings;
struct Scheduler;
struct ExternalDatabases;
struct ThreadLocals;

struct GlobalsAdapter {
  SceneState* scene{};
  PhysicsTableIds* physicsTables{};
  FileSystem* fileSystem{};
  StableElementMappings* stableMappings{};
  ConstraintsTableMappings* constraintsMappings{};
  Scheduler* scheduler{};
  ExternalDatabases* externalDB{};
  ThreadLocals* threadLocals{};
};

struct CentralStatEffectAdapter {
  PositionStatEffectAdapter position;
  VelocityStatEffectAdapter velocity;
  LambdaStatEffectAdapter lambda;
};

namespace TableAdapters {
  ConfigAdapter getConfig(GameDB db);
  StableElementMappings& getStableMappings(GameDB db);
  ThreadLocals& getThreadLocals(GameDB db);
  ThreadLocalData getThreadLocal(GameDB db, size_t thread);
  ExternalDatabases& getExternalDBs(GameDB db);
  StatEffectDBOwned& getStatEffects(GameDB db);

  PositionStatEffectAdapter getPositionEffects(GameDB db, size_t thread);
  VelocityStatEffectAdapter getVelocityEffects(GameDB db, size_t thread);
  LambdaStatEffectAdapter getLambdaEffects(GameDB db, size_t thread);
  AreaForceStatEffectAdapter getAreaForceEffects(GameDB db, size_t thread);
  CentralStatEffectAdapter getCentralStatEffects(GameDB db);

  GameObjectAdapter getGameObjects(GameDB db);
  GameObjectAdapter getStaticGameObjects(GameDB db);
  PlayerAdapter getPlayer(GameDB db);
  //The gameplay extracted versions of the above
  GameObjectAdapter getGameplayGameObjects(GameDB db);
  GameObjectAdapter getGameplayStaticGameObjects(GameDB db);
  PlayerAdapter getGameplayPlayer(GameDB db);

  size_t addStatEffectsSharedLifetime(StatEffectBaseAdapter& base, size_t lifetime, const size_t* stableIds, size_t count);

  GlobalsAdapter getGlobals(GameDB db);
};