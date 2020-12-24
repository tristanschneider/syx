#include "Precompile.h"
#include "test/TestKeyboardInput.h"

KeyState TestKeyboardInputImpl::getKeyState(Key key) const {
  auto it = mKeyStates.find(key);
  return it != mKeyStates.end() ? it->second : KeyState::Up;
}

void TestKeyboardInputImpl::update() {
  if(mUpdatesUntilInputClear && (*mUpdatesUntilInputClear)-- <= 0) {
    clear();
  }
}

TestKeyboardInputImpl& TestKeyboardInputImpl::clearInputAfterOneFrame() {
  mUpdatesUntilInputClear = 1;
  return *this;
}

TestKeyboardInputImpl& TestKeyboardInputImpl::clear() {
  mMousePos = Syx::Vec2::sZero;
  mMouseDelta = Syx::Vec2::sZero;
  mWheelDelta = 0.f;
  mKeyStates.clear();
  mUpdatesUntilInputClear.reset();
  return *this;
}

Syx::Vec2 TestKeyboardInputImpl::getMousePos() const {
  return mMousePos;
}

Syx::Vec2 TestKeyboardInputImpl::getMouseDelta() const {
  return mMouseDelta;
}

float TestKeyboardInputImpl::getWheelDelta() const {
  return mWheelDelta;
}