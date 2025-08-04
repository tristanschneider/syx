#pragma once

namespace ShapeRegistry {
  struct IShapeRegistry;
}
namespace Shapes {
  void registerDefaultShapes(ShapeRegistry::IShapeRegistry& registry);
}