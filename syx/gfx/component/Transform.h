#pragma once
#include "Component.h"

class Transform : public Component {
public:
  Transform(Handle owner, MessageQueueProvider* messaging);

  void set(const Syx::Mat4& m, bool fireEvent = true);
  const Syx::Mat4& get();

private:
  void _fireEvent();

  Syx::Mat4 mMat;
};