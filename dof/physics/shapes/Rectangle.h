#pragma once

#include "RuntimeDatabase.h"
#include "shapes/ShapeRegistry.h"

namespace ShapeRegistry {
  struct IShapeImpl;
}

namespace Shapes {
  struct RectDefinition {
    //Required
    ConstFloatQueryAlias centerX;
    ConstFloatQueryAlias centerY;
    //Optional, default no rotation
    ConstFloatQueryAlias rotX;
    ConstFloatQueryAlias rotY;
    //Optional, default unit square
    ConstFloatQueryAlias scaleX;
    ConstFloatQueryAlias scaleY;
  };
  //Table is all unit cubes. Currently only supports the single RectDefinition
  struct SharedRectangleRow : TagRow {};
  struct RectangleRow : Row<ShapeRegistry::Rectangle> {};

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualRectangle();
  std::unique_ptr<ShapeRegistry::IShapeImpl> createSharedRectangle(const RectDefinition& rect);
}