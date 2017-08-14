#pragma once
#include "Component.h"

class TransformComponent : public Component {
public:
  TransformComponent(Handle owner);

  Handle getHandle() const override {
    return static_cast<Handle>(ComponentType::Graphics);
  }

  Syx::Mat4 mMat;
};