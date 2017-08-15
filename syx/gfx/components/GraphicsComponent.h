#pragma once
#include "Component.h"

class GraphicsComponent : public Component {
public:
  GraphicsComponent(Handle owner);

  Handle getHandle() const override {
    return static_cast<Handle>(ComponentType::Graphics);
  }

  Handle mModel, mDiffTex;
};