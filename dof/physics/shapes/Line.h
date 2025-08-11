#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  struct LineRow : TagRow {};

  ShapeRegistry::Raycast lineFromTransform(const Transform::PackedTransform& transform);
  Transform::PackedTransform toTransform(const ShapeRegistry::Raycast& v, float z = 0);

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualLine();
}