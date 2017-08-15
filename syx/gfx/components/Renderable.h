#pragma once
#include "Component.h"

class Renderable : public Component {
public:
  Renderable(Handle owner);

  Handle getHandle() const override {
    return static_cast<Handle>(ComponentType::Graphics);
  }

  Handle mModel, mDiffTex;
};