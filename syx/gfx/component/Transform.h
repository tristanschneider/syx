#pragma once
#include "Component.h"

class MessagingSystem;

class Transform : public Component {
public:
  Transform(Handle owner, MessagingSystem* messaging);

  Handle getHandle() const override {
    return static_cast<Handle>(ComponentType::Graphics);
  }

  void set(const Syx::Mat4& m, bool fireEvent = true);
  const Syx::Mat4& get();

private:
  void _fireEvent();

  Syx::Mat4 mMat;
};