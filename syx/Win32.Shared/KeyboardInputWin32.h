#pragma once
#include "system/KeyboardInput.h"

class KeyboardInputWin32 : public KeyboardInputImpl {
public:
  KeyboardInputWin32();

  void feedWheelDelta(float delta);

  void update() override;

  KeyState getKeyState(Key key) const override;
  Syx::Vec2 getMousePos() const override;
  Syx::Vec2 getMouseDelta() const override;
  float getWheelDelta() const override;

private:
  static const size_t sKeyCount = 256;
  static const size_t sAsciiCount = 128;

  uint8_t mPrevState[sKeyCount];
  uint8_t mCurState[sKeyCount];
  Syx::Vec2 mPrevMouse;
  Syx::Vec2 mCurMouse;
  float mPrevWheel;
  float mCurWheel;
};