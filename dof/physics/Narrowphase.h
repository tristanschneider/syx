#pragma once

#include "RuntimeDatabase.h"
#include "Table.h"
#include "glm/vec2.hpp"

class IAppBuilder;
struct PhysicsAliases;

namespace Narrowphase {
  namespace Shape {
    struct UnitCube {
      glm::vec2 center{};
      glm::vec2 right{};
    };
    struct Raycast {
      glm::vec2 start{};
      glm::vec2 end{};
    };
    struct AABB {
      glm::vec2 min{};
      glm::vec2 max{};
    };
    struct Circle {
      glm::vec2 pos{};
      float radius{};
    };
    using Variant = std::variant<std::monostate, UnitCube, Raycast, AABB, Circle>;

    struct BodyType {
      Shape::Variant shape;
    };
  };

  struct UnitCubeDefinition {
    ConstFloatQueryAlias centerX;
    ConstFloatQueryAlias centerY;
    ConstFloatQueryAlias rotX;
    ConstFloatQueryAlias rotY;
  };

  //Table is all unit cubes. Currently only supports the single UnitCubeDefinition
  struct SharedUnitCubeRow : TagRow {};
  struct UnitCubeRow : Row<Shape::UnitCube> {};
  struct RaycastRow : Row<Shape::Raycast> {};
  struct AABBRow : Row<Shape::AABB> {};
  struct CircleRow : Row<Shape::Circle> {};
  struct CollisionMaskRow : Row<uint8_t> {};

  //Takes the pairs stored in SpatialPairsTable and generates the contacts needed to resolve the spatial
  // queries or constraint solving
  void generateContactsFromSpatialPairs(IAppBuilder& builder, const UnitCubeDefinition& unitCube);
}