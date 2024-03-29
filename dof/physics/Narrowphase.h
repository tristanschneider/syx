#pragma once

#include "RuntimeDatabase.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include <variant>

class IAppBuilder;
struct PhysicsAliases;
class RuntimeDatabaseTaskBuilder;

namespace Narrowphase {
  namespace Shape {
    struct Rectangle {
      glm::vec2 center{};
      glm::vec2 right{};
      glm::vec2 halfWidth{ 0.5f };
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
    using Variant = std::variant<std::monostate, Rectangle, Raycast, AABB, Circle>;

    //The center that "centerToContact" in the manifold is relative to
    glm::vec2 getCenter(const Variant& shape);

    struct BodyType {
      Shape::Variant shape;
    };
  };

  struct RectDefinition {
    //Required
    ConstFloatQueryAlias centerX;
    ConstFloatQueryAlias centerY;
    //Optional, default no rotation
    ConstFloatQueryAlias rotX;
    ConstFloatQueryAlias rotY;
    //Optional, default unit square
    ConstFloatQueryAlias scaleX;
    ConstFloatQueryAlias scaleY;
  };

  using CollisionMask = uint8_t;

  //Table is all unit cubes. Currently only supports the single UnitCubeDefinition
  struct SharedRectangleRow : TagRow {};
  struct RectangleRow : Row<Shape::Rectangle> {};
  struct RaycastRow : Row<Shape::Raycast> {};
  struct AABBRow : Row<Shape::AABB> {};
  struct CircleRow : Row<Shape::Circle> {};
  struct CollisionMaskRow : Row<CollisionMask> {};
  //Any of the above shapes can optionally have a thickness and Z coordinate
  //Without it, shapes are considered to have zero thickness at z=0
  //The value represents the distance from the bottom of the shape to the top of the shape.
  //The center of the object on the Z axis is the Z position + half thickness
  struct SharedThicknessRow : SharedRow<float> {};
  struct ThicknessRow : Row<float> {};
  constexpr float DEFAULT_THICKNESS = 0.0f;

  struct IShapeClassifier {
    virtual ~IShapeClassifier() = default;
    virtual Shape::BodyType classifyShape(const UnpackedDatabaseElementID& id) = 0;
  };

  std::shared_ptr<IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, const RectDefinition& rect, const PhysicsAliases& aliases);

  //Takes the pairs stored in SpatialPairsTable and generates the contacts needed to resolve the spatial
  // queries or constraint solving
  void generateContactsFromSpatialPairs(IAppBuilder& builder, const RectDefinition& rect, const PhysicsAliases& aliases);
}