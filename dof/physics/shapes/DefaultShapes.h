#pragma once

namespace ShapeRegistry {
  struct IShapeRegistry;
}
namespace Shapes {
  struct RectDefinition;

  void registerDefaultShapes(ShapeRegistry::IShapeRegistry& registry, const RectDefinition& rect);
}