#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "Table.h"

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
struct TableID;

namespace Config {
  struct GameConfig;
  struct PhysicsConfig;
}

struct ConfigAdapter {
  Config::PhysicsConfig* physics{};
  Config::GameConfig* game{};
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
  size_t getThreadCount(RuntimeDatabaseTaskBuilder& task);

  DebugLineAdapter getDebugLines(RuntimeDatabaseTaskBuilder& task);
  const float* getDeltaTime(RuntimeDatabaseTaskBuilder& task);

  PhysicsObjectAdapter getPhysics(RuntimeDatabaseTaskBuilder& task, const TableID& table);
  PhysicsObjectAdapter getGameplayPhysics(RuntimeDatabaseTaskBuilder& task, const TableID& table);
  GameObjectAdapter getGameObject(RuntimeDatabaseTaskBuilder& task, const TableID& table);
  GameObjectAdapter getGameplayGameObject(RuntimeDatabaseTaskBuilder& task, const TableID& table);

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