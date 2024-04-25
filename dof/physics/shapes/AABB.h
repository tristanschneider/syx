#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  struct AABBRow : Row<ShapeRegistry::AABB> {};

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualAABB();
}