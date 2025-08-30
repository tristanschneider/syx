#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  struct CircleRow : TagRow {};

  ShapeRegistry::Circle circleFromTransform(const Transform::PackedTransform& t);
  Transform::PackedTransform toTransform(const ShapeRegistry::Circle& c, float z = 0.f);

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualCircle();
}