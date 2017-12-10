#include "Precompile.h"
#include "KeyboardInput.h"
#include <Windows.h>

extern HWND gHwnd;

RegisterSystemCPP(KeyboardInput);

KeyState KeyboardInput::getKeyState(Key key) const {
  //Only care about top bit
  unsigned char filter = 128;
  unsigned char prev = mPrevState[static_cast<int>(key)] & filter;
  unsigned char cur = mCurState[static_cast<int>(key)] & filter;
  if(prev != cur)
    return prev ? KeyState::Released : KeyState::Triggered;
  return cur ? KeyState::Down : KeyState::Up;
}

bool KeyboardInput::getKeyDown(Key key) const {
  return (mCurState[static_cast<int>(key)] & 128) != 0;
}

bool KeyboardInput::getKeyUp(Key key) const {
  return (mCurState[static_cast<int>(key)] & 128) == 0;
}

bool KeyboardInput::getKeyTriggered(Key key) const {
  return getKeyState(key) == KeyState::Triggered;
}

bool KeyboardInput::getKeyReleased(Key key) const {
  return getKeyState(key) == KeyState::Released;
}

static Key _charToKey(char c, char baseChar, Key baseKey) {
  uint8_t offset = static_cast<uint8_t>(c - baseChar);
  return static_cast<Key>(static_cast<uint8_t>(baseKey) + offset);
}

KeyState KeyboardInput::getAsciiState(char c) const {
  switch(c) {
    case '!': return _shiftAnd(Key::Key1);
    case '@': return _shiftAnd(Key::Key2);
    case '#': return _shiftAnd(Key::Key3);
    case '$': return _shiftAnd(Key::Key4);
    case '%': return _shiftAnd(Key::Key5);
    case '^': return _shiftAnd(Key::Key6);
    case '&': return _shiftAnd(Key::Key7);
    case '*': return _shiftAnd(Key::Key8);
    case '(': return _shiftAnd(Key::Key9);
    case ')': return _shiftAnd(Key::Key0);
    case '-': return _noShift(Key::MinusUnderLine);
    case '_': return _shiftAnd(Key::MinusUnderLine);
    case '+': return _shiftAnd(Key::PlusEq);
    case '=': return _noShift(Key::PlusEq);
    case '[': return _noShift(Key::LeftCurly);
    case '{': return _shiftAnd(Key::LeftCurly);
    case ']': return _noShift(Key::RightCurly);
    case '}': return _shiftAnd(Key::RightCurly);
    case '\\': return _noShift(Key::Bar);
    case '|': return _shiftAnd(Key::Bar);
    case ';': return _noShift(Key::Semicolon);
    case ':': return _shiftAnd(Key::Semicolon);
    case '\'': return _noShift(Key::Quote);
    case '"': return _shiftAnd(Key::Quote);
    case ',': return _noShift(Key::Comma);
    case '<': return _shiftAnd(Key::Comma);
    case '.': return _noShift(Key::Dot);
    case '>': return _shiftAnd(Key::Dot);
    case '/': return _noShift(Key::Question);
    case '?': return _shiftAnd(Key::Question);
    case '`': return _noShift(Key::Tilda);
    case '~': return _shiftAnd(Key::Tilda);
    case ' ': return _or(_noShift(Key::Space), _shiftAnd(Key::Space));
  }

  // Key codes numbers and characters match
  if(c >= 'a' && c <= 'z')
    return _noShift(_charToKey(c, 'a', Key::KeyA));
  if(c >= 'A' && c <= 'Z')
    return _shiftAnd(_charToKey(c, 'A', Key::KeyA));
  if(c >= '0' && c <= '9')
    return _noShift(_charToKey(c, '0', Key::Key0));
  return KeyState::Up;
}

KeyState KeyboardInput::_shiftAnd(Key key) const {
  if(getKeyDown(Key::Shift))
    return getKeyState(key);
  return KeyState::Up;
}

KeyState KeyboardInput::_noShift(Key key) const {
  if(getKeyDown(Key::Shift))
    return KeyState::Up;
  return getKeyState(key);
}

KeyState KeyboardInput::_or(KeyState a, KeyState b) const {
  if(a == KeyState::Triggered || b == KeyState::Triggered)
    return KeyState::Triggered;
  if(a == KeyState::Down || b == KeyState::Down)
    return KeyState::Down;
  return KeyState::Up;
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

void KeyboardInput::update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  std::memcpy(mPrevState, mCurState, sKeyCount);
  GetKeyboardState(mCurState);
  POINT p;
  if(GetCursorPos(&p)) {
    ScreenToClient(gHwnd, &p);
    mPrevMouse = mCurMouse;
    mCurMouse = Syx::Vec2(static_cast<float>(p.x), static_cast<float>(p.y));
  }
}

void KeyboardInput::uninit() {
}