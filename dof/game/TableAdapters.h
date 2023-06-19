#pragma once

#include "TableOperations.h"

struct CurveDefinition;
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
  PhysicsConfig* physics{};
  GameConfig* game{};
};

namespace StatEffect {
  struct Owner;
  struct Lifetime;
  struct Global;
  struct Target;
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
  struct CommandRow;
};

namespace FollowTargetByPositionStatEffect {
  struct CommandRow;
}

struct StatEffectBaseAdapter {
  StatEffect::Owner* owner{};
  StatEffect::Lifetime* lifetime{};
  StatEffect::Global* global{};
  StableTableModifierInstance modifier;

  //Optionals
  StatEffect::Target* target{};
  Row<float>* curveInput{};
  Row<float>* curveOutput{};
  Row<CurveDefinition*>* curveDefinition{};
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
  AreaForceStatEffect::CommandRow* command{};
};

struct FollowTargetByPositionStatEffectAdapter {
  StatEffectBaseAdapter base;
  FollowTargetByPositionStatEffect::CommandRow* command{};
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

struct CameraAdapater {
  GameObjectAdapter object;
};

struct SceneState;
struct PhysicsTableIds;
struct FileSystem;
struct StableElementMappings;
struct ConstraintsTableMappings;
struct Scheduler;
struct ExternalDatabases;
struct ThreadLocals;

struct DebugPoint;

using DebugLineTable = Table<Row<DebugPoint>>;

struct DebugLineAdapter {
  BasicRow<DebugPoint>* points{};
  TableModifierInstance modifier;
};

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
  FollowTargetByPositionStatEffectAdapter followTargetByPosition;
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
  FollowTargetByPositionStatEffectAdapter getFollowTargetByPositionEffects(GameDB db, size_t thread);
  CentralStatEffectAdapter getCentralStatEffects(GameDB db);


  GameObjectAdapter getGameObjects(GameDB db);
  GameObjectAdapter getStaticGameObjects(GameDB db);
  PlayerAdapter getPlayer(GameDB db);
  //The gameplay extracted versions of the above
  GameObjectAdapter getGameplayGameObjects(GameDB db);
  GameObjectAdapter getGameplayStaticGameObjects(GameDB db);
  PlayerAdapter getGameplayPlayer(GameDB db);
  CameraAdapater getCamera(GameDB db);

  DebugLineAdapter getDebugLines(GameDB db);

  size_t addStatEffectsSharedLifetime(StatEffectBaseAdapter& base, size_t lifetime, const size_t* stableIds, size_t count);

  GlobalsAdapter getGlobals(GameDB db);
};