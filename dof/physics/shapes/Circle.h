#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  struct CircleRow : TagRow {};

  ShapeRegistry::Circle circleFromTransform(const Transform::PackedTransform& t);

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualCircle();
}