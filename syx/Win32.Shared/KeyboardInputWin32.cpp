#include "Precompile.h"
#include "KeyboardInputWin32.h"

#include <Windows.h>

extern HWND gHwnd;

KeyboardInputWin32::KeyboardInputWin32() {
  std::memset(mPrevState, 0, sKeyCount);
  std::memset(mCurState, 0, sKeyCount);
}

KeyState KeyboardInputWin32::getKeyState(Key key) const {
  //Only care about top bit
  unsigned char filter = 128;
  unsigned char prev = mPrevState[static_cast<int>(key)] & filter;
  unsigned char cur = mCurState[static_cast<int>(key)] & filter;
  if(prev != cur)
    return prev ? KeyState::Released : KeyState::Triggered;
  return cur ? KeyState::Down : KeyState::Up;
}

Syx::Vec2 KeyboardInputWin32::getMousePos() const {
  return mCurMouse;
}

Syx::Vec2 KeyboardInputWin32::getMouseDelta() const {
  return mCurMouse - mPrevMouse;
}

float KeyboardInputWin32::getWheelDelta() const {
  return mCurWheel;
}

void KeyboardInputWin32::update() {
  std::memcpy(mPrevState, mCurState, sKeyCount);
  GetKeyboardState(mCurState);
  POINT p;
  if(GetCursorPos(&p)) {
    ScreenToClient(gHwnd, &p);
    mPrevMouse = mCurMouse;
    mCurMouse = Syx::Vec2(static_cast<float>(p.x), static_cast<float>(p.y));
  }
  mCurWheel = mPrevWheel;
  mPrevWheel = 0.0f;
}

void KeyboardInputWin32::feedWheelDelta(float delta) {
  mPrevWheel = delta;
}
