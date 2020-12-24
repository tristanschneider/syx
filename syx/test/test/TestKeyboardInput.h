#pragma once
#include <optional>

#include "system/KeyboardInput.h"

class TestKeyboardInputImpl : public KeyboardInputImpl {
public:
  virtual KeyState getKeyState(Key key) const override;
  virtual void update() override;
  virtual Syx::Vec2 getMousePos() const override;
  virtual Syx::Vec2 getMouseDelta() const override;
  virtual float getWheelDelta() const override;

  //Tests are intended to set these values as needed to mock inputs
  Syx::Vec2 mMousePos = Syx::Vec2::sZero;
  Syx::Vec2 mMouseDelta = Syx::Vec2::sZero;
  float mWheelDelta = 0.f;
  //If the value is present in this map it is returned in getKeyState, otherwise it's "up"
  std::unordered_map<Key, KeyState> mKeyStates;
  //If this is set, all inputs will be cleared upon the input system being updated. This is before gameplay and editor is updated, so if setting from a test
  //this should be set to 1 to allow one update to pass with the input being simulated, then cleared on the following update, causing one frame of simulated input
  std::optional<int> mUpdatesUntilInputClear;

  TestKeyboardInputImpl& clearInputAfterOneFrame();
  TestKeyboardInputImpl& clear();
};
