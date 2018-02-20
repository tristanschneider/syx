#pragma once
#include "Component.h"

class Transform : public Component {
public:
  Transform(Handle owner);

  void set(const Syx::Mat4& m);
  const Syx::Mat4& get();

private:
  Syx::Mat4 mMat;
};