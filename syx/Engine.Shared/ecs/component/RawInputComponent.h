#pragma once

#include <variant>

//Same as windows keys, but this way you don't have to include windows.h to query key states
enum class Key : uint8_t {
  LeftMouse = 1,
  RightMouse = 2,
  MiddleMouse = 4,
  XMouse1 = 5,
  Backspace = 8,
  Tab = 9,
  Enter = 13,
  Shift = 16,
  Control = 17,
  Alt = 18,
  CapsLock = 20,
  Esc = 27,
  Space = 32,
  PageUp = 33,
  PageDown = 34,
  End = 35,
  Home = 36,
  Left = 37,
  Up = 38,
  Right = 39,
  Down = 40,
  Delete = 46,
  //Not numpad
  Key0 = 48, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
  KeyA = 65, KeyB, KeyC, KeyD, KeyE, KeyF, KeyG, KeyH, KeyI, KeyJ, KeyK, KeyL, KeyM, KeyN, KeyO, KeyP, KeyQ, KeyR, KeyS, KeyT, KeyU, KeyV, KeyW, KeyX, KeyY, KeyZ,
  //Numpad
  Num0 = 96, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
  Mul = 106,
  Add = 107,
  Sub = 109,
  Dot = 0xBE,
  FwdSlash = 111,
  Semicolon = 0xBA,
  Comma = 0xBC,
  Question = 0xBF,
  Tilda = 0xC0,
  LeftCurly = 0xDB,
  Bar = 0xDC,
  RightCurly = 0XDD,
  Quote = 0xDE,
  MinusUnderLine = 0xBD,
  PlusEq = 0xBB,
  F1 = 112, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
  LeftShift = 160,
  RightShift,
  LeftCtrl,
  RightCtrl,
  Count = 255
};

enum class KeyState : uint8_t {
  Up,
  Down,
  Triggered,
  Released,
  Invalid
};

struct RawInputEvent {
  struct KeyEvent {
    Key mKey = Key::Count;
    KeyState mState = KeyState::Invalid;
  };

  struct MouseKeyEvent {
    Key mKey = Key::Count;
    KeyState mState = KeyState::Invalid;
    Syx::Vec2 mPos;
  };

  struct MouseMoveEvent {
    Syx::Vec2 mPos;
    Syx::Vec2 mDelta;
  };

  struct MouseWheelEvent {
    float mAmount = 0;
  };

  std::variant<KeyEvent, MouseKeyEvent, MouseMoveEvent, MouseWheelEvent> mData;
};

//Updated by the OS, processed into the RawInputComponent by the InputSystem
struct RawInputBufferComponent {
  std::vector<RawInputEvent> mEvents;
};

//The current interpretation of input state from OS input events. View this for raw input state
struct RawInputComponent {
  KeyState getKeyState(Key key) const {
    const auto it = mKeyStates.find(key);
    return it != mKeyStates.end() ? it->second : KeyState::Up;
  }

  bool getKeyDown(Key key) const {
    return getKeyState(key) == KeyState::Down;
  }

  //All key states with abscence implying released
  std::unordered_map<Key, KeyState> mKeyStates;
  Syx::Vec2 mMousePos;
  Syx::Vec2 mMouseDelta;
  float mWheelDelta = 0.f;
};