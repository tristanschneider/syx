#include "Precompile.h"
#include "KeyboardInput.h"
#include <Windows.h>

KeyboardInput::KeyState KeyboardInput::getKeyState(Key key) const {
  //Only care about top bit
  unsigned char filter = 128;
  unsigned char prev = mPrevState[static_cast<int>(key)] & filter;
  unsigned char cur = mCurState[static_cast<int>(key)] & filter;
  if(prev != cur)
    return prev ? KeyState::OnUp : KeyState::OnDown;
  return cur ? KeyState::Down : KeyState::Up;
}

Syx::Vec2 KeyboardInput::getMousePos() const {
  return mCurMouse;
}

Syx::Vec2 KeyboardInput::getMouseDelta() const {
  return mCurMouse - mPrevMouse;
}

void KeyboardInput::init() {
  std::memset(mPrevState, 0, sKeyCount);
  std::memset(mCurState, 0, sKeyCount);
}

void KeyboardInput::update(float dt) {
  std::memcpy(mPrevState, mCurState, sKeyCount);
  GetKeyboardState(mCurState);
  POINT p;
  if(GetCursorPos(&p)) {
    mPrevMouse = mCurMouse;
    mCurMouse = Syx::Vec2(static_cast<float>(p.x), static_cast<float>(p.y));
  }
}

void KeyboardInput::uninit() {

}
