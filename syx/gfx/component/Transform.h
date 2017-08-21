#pragma once
#include "Component.h"

class MessagingSystem;

class Transform : public Component {
public:
  Transform(Handle owner, MessagingSystem* messaging);

  Handle getHandle() const override {
    return static_cast<Handle>(ComponentType::Graphics);
  }

  void set(const Syx::Mat4& m);
  const Syx::Mat4& get();

private:
  void fireEvent();

  Syx::Mat4 mMat;
};