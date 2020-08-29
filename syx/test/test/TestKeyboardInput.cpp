#include "Precompile.h"
#include "test/TestKeyboardInput.h"

KeyState TestKeyboardInputImpl::getKeyState(Key) const {
  return KeyState::Up;
}

void TestKeyboardInputImpl::update() {
}

Syx::Vec2 TestKeyboardInputImpl::getMousePos() const {
  return Syx::Vec2(0, 0);
}

Syx::Vec2 TestKeyboardInputImpl::getMouseDelta() const {
  return Syx::Vec2(0, 0);
}

float TestKeyboardInputImpl::getWheelDelta() const {
  return 0.0f;
}