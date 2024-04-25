#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  struct LineRow : Row<ShapeRegistry::Raycast> {};

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualLine();
}