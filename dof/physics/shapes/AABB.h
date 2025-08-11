#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  struct AABBRow : TagRow {};

  ShapeRegistry::AABB aabbFromTransform(const Transform::PackedTransform& t);
  const Transform::PackedTransform toTransform(const ShapeRegistry::AABB& bb, float z = 0);

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualAABB();
}