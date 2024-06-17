#pragma once

#include "RuntimeDatabase.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include <variant>
#include "shapes/ShapeRegistry.h"

class IAppBuilder;
struct PhysicsAliases;
class RuntimeDatabaseTaskBuilder;

namespace ShapeRegistry {
  struct BodyType;
  struct IShapeClassifier;
};

namespace Narrowphase {
  //This indirection has no practical significance, I moved types around and didn't wnat to update use locations
  namespace Shape {
    using Rectangle = ShapeRegistry::Rectangle;
    using Raycast = ShapeRegistry::Raycast;
    using AABB = ShapeRegistry::AABB;
    using Circle = ShapeRegistry::Circle;
    using BodyType = ShapeRegistry::BodyType;
  };
  using IShapeClassifier = ShapeRegistry::IShapeClassifier;

  using CollisionMask = uint8_t;

  struct CollisionMaskRow : Row<CollisionMask> {};
  //Any of the above shapes can optionally have a thickness and Z coordinate
  //Without it, shapes are considered to have zero thickness at z=0
  //The value represents the distance from the bottom of the shape to the top of the shape.
  //The center of the object on the Z axis is the Z position + half thickness
  struct SharedThicknessRow : SharedRow<float> {};
  struct ThicknessRow : Row<float> {};
  constexpr float DEFAULT_THICKNESS = 0.0f;
  constexpr float Z_OVERLAP_TOLERANCE = 0.01f;

  //Takes the pairs stored in SpatialPairsTable and generates the contacts needed to resolve the spatial
  //queries or constraint solving
  void generateContactsFromSpatialPairs(IAppBuilder& builder, const PhysicsAliases& aliases, size_t threadCount);
}