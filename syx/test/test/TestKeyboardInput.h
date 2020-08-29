#pragma once

#include "system/KeyboardInput.h"

class TestKeyboardInputImpl : public KeyboardInputImpl {
public:
  virtual KeyState getKeyState(Key key) const override;
  virtual void update() override;
  virtual Syx::Vec2 getMousePos() const override;
  virtual Syx::Vec2 getMouseDelta() const override;
  virtual float getWheelDelta() const override;
};
