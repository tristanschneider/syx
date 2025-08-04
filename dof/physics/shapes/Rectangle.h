#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  //Rectangles use transform to determine their shape. Scale is vector from center to corner.
  struct RectangleRow : TagRow {};

  std::unique_ptr<ShapeRegistry::IShapeImpl> createRectangle();
}