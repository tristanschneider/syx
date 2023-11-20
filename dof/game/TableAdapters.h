#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "TableOperations.h"

struct AppTaskArgs;
struct CurveDefinition;
struct StableIDRow;
struct ThreadLocals;
class ITableModifier;
class RuntimeDatabaseTaskBuilder;
struct ThreadLocals;
struct DebugPoint;
struct DebugText;
template<class Element>
struct BasicRow;

namespace Config {
  struct GameConfig;
  struct PhysicsConfig;
}

struct ConfigAdapter {
  Config::PhysicsConfig* physics{};
  Config::GameConfig* game{};
};

namespace StatEffect {
  struct Owner;
  struct Lifetime;
  struct Global;
  struct Target;
  struct Continuations;
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

namespace FollowTargetByVelocityStatEffect {
  struct CommandRow;
}

namespace DamageStatEffect {
  struct CommandRow;
};

struct StatEffectBaseAdapter {
  StatEffect::Owner* owner{};
  StatEffect::Lifetime* lifetime{};
  StatEffect::Global* global{};
  StatEffect::Continuations* continuations{};
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

struct FollowTargetByVelocityStatEffectAdapter {
  StatEffectBaseAdapter base;
  FollowTargetByVelocityStatEffect::CommandRow* command{};
};

struct DamageStatEffectAdapter {
  StatEffectBaseAdapter base;
  DamageStatEffect::CommandRow* command{};
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

  BasicRow<uint8_t>* collisionMask{};
};

struct GameObjectAdapter {
  TransformAdapter transform;
  PhysicsObjectAdapter physics;
  const StableIDRow* stable{};
};

struct DebugLineAdapter {
  ~DebugLineAdapter();

  BasicRow<DebugPoint>* points{};
  BasicRow<DebugText>* text{};
  std::shared_ptr<ITableModifier> pointModifier;
  std::shared_ptr<ITableModifier> textModifier;
};

namespace TableAdapters {
  const Config::PhysicsConfig* getPhysicsConfig(RuntimeDatabaseTaskBuilder& task);
  Config::PhysicsConfig* getPhysicsConfigMutable(RuntimeDatabaseTaskBuilder& task);
  const Config::GameConfig* getGameConfig(RuntimeDatabaseTaskBuilder& task);
  Config::GameConfig* getGameConfigMutable(RuntimeDatabaseTaskBuilder& task);

  ThreadLocals& getThreadLocals(RuntimeDatabaseTaskBuilder& task);

  VelocityStatEffectAdapter getVelocityEffects(AppTaskArgs& args);
  PositionStatEffectAdapter getPositionEffects(AppTaskArgs& args);
  LambdaStatEffectAdapter getLambdaEffects(AppTaskArgs& args);
  AreaForceStatEffectAdapter getAreaForceEffects(AppTaskArgs& args);
  FollowTargetByPositionStatEffectAdapter getFollowTargetByPositionEffects(AppTaskArgs& args);
  FollowTargetByVelocityStatEffectAdapter getFollowTargetByVelocityEffects(AppTaskArgs& args);
  DamageStatEffectAdapter getDamageEffects(AppTaskArgs& args);

  DebugLineAdapter getDebugLines(RuntimeDatabaseTaskBuilder& task);
  const float* getDeltaTime(RuntimeDatabaseTaskBuilder& task);

  size_t addStatEffectsSharedLifetime(StatEffectBaseAdapter& base, size_t lifetime, const size_t* stableIds, size_t count);

  TransformAdapter getTransform(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table);
  TransformAdapter getGameplayTransform(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table);
  PhysicsObjectAdapter getPhysics(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table);
  PhysicsObjectAdapter getGameplayPhysics(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table);
  GameObjectAdapter getGameObject(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table);
  GameObjectAdapter getGameplayGameObject(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table);

  inline glm::vec2 read(size_t i, const Row<float>& a, const Row<float>& b) {
    return { a.at(i), b.at(i) };
  }

  template<size_t I, class TupleT>
  glm::vec2 read(size_t i, const TupleT& t) {
    return read(i, std::get<I>(t), std::get<I + 1>(t));
  }

  inline void write(size_t i, const glm::vec2& v, Row<float>& x, Row<float>& y) {
    x.at(i) = v.x;
    y.at(i) = v.y;
  }

  inline void add(size_t i, const glm::vec2& v, Row<float>& x, Row<float>& y) {
    x.at(i) += v.x;
    y.at(i) += v.y;
  }

  inline void add(size_t i, float v, Row<float>& x) {
    x.at(i) += v;
  }
};