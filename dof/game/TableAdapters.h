#pragma once

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
  static CentralStatEffectAdapter getCentralStatEffects(GameDB db);

  static GameObjectAdapter getGameObjects(GameDB db);
  static GameObjectAdapter getStaticGameObjects(GameDB db);
  static PlayerAdapter getPlayer(GameDB db);
  //The gameplay extracted versions of the above
  static GameObjectAdapter getGameplayGameObjects(GameDB db);
  static GameObjectAdapter getGameplayStaticGameObjects(GameDB db);
  static PlayerAdapter getGameplayPlayer(GameDB db);

  static GlobalsAdapter getGlobals(GameDB db);
};