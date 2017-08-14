#pragma once
#include "Component.h"

class GraphicsComponent : public Component {
public:
  GraphicsComponent(Handle owner);

  Handle getHandle() {
    return static_cast<Handle>(ComponentType::Graphics);
  }

  Handle mModel, mDiffTex;
};