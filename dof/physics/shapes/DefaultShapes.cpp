#include "shapes/DefaultShapes.h"

#include "shapes/AABB.h"
#include "shapes/Circle.h"
#include "shapes/Line.h"
#include "shapes/Rectangle.h"
#include "shapes/ShapeRegistry.h"
#include <shapes/Mesh.h>

namespace Shapes {
  void registerDefaultShapes(ShapeRegistry::IShapeRegistry& registry) {
    registry.registerImpl(createIndividualAABB());
    registry.registerImpl(createIndividualCircle());
    registry.registerImpl(createIndividualLine());
    registry.registerImpl(createRectangle());
    registry.registerImpl(createMesh());
    registry.registerImpl(createStaticTriangleMesh());
  }
}