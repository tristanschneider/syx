#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  struct CircleRow : Row<ShapeRegistry::Circle> {};

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualCircle();
}