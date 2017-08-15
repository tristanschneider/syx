#pragma once
#include "Component.h"

class Transform : public Component {
public:
  Transform(Handle owner);

  Handle getHandle() const override {
    return static_cast<Handle>(ComponentType::Graphics);
  }

  Syx::Mat4 mMat;
};