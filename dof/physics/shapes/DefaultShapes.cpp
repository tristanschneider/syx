#include "Precompile.h"
#include "shapes/DefaultShapes.h"

#include "shapes/AABB.h"
#include "shapes/Circle.h"
#include "shapes/Line.h"
#include "shapes/Rectangle.h"
#include "shapes/ShapeRegistry.h"

namespace Shapes {
  void registerDefaultShapes(ShapeRegistry::IShapeRegistry& registry, const RectDefinition& rect) {
    registry.registerImpl(createIndividualAABB());
    registry.registerImpl(createIndividualCircle());
    registry.registerImpl(createIndividualLine());
    registry.registerImpl(createIndividualRectangle());
    registry.registerImpl(createSharedRectangle(rect));
  }
}