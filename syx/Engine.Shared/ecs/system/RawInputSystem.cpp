#include "Precompile.h"
#include "ecs/system/RawInputSystem.h"

#include "ecs/component/RawInputComponent.h"

namespace {
  KeyState _shiftAnd(const RawInputComponent& input, Key key) {
    if(input.getKeyDown(Key::Shift))
      return KeyState::Up;
    return input.getKeyState(key);
  }

  KeyState _noShift(const RawInputComponent& input, Key key) {
    if(input.getKeyDown(Key::Shift))
      return KeyState::Up;
    return input.getKeyState(key);
  }

  KeyState _or(KeyState a, KeyState b) {
    if(a == KeyState::Triggered || b == KeyState::Triggered)
      return KeyState::Triggered;
    if(a == KeyState::Down || b == KeyState::Down)
      return KeyState::Down;
    return KeyState::Up;
  }

  const std::unordered_map<Key, std::string> KEY_TO_STRING = []() {
    std::unordered_map<Key, std::string> map;
    map.reserve(static_cast<size_t>(Key::Count));
      map[Key::LeftMouse] = "lmb";
      map[Key::RightMouse] = "rmb";
      map[Key::MiddleMouse] = "mmb";
      map[Key::Backspace] = "backspace";
      map[Key::Tab] = "tab";
      map[Key::Enter] = "enter";
      map[Key::Shift] = "shift";
      map[Key::Control] = "ctrl";
      map[Key::Alt] = "alt";
      map[Key::CapsLock] = "caps";
      map[Key::Esc] = "esc";
      map[Key::Space] = "space";
      map[Key::PageUp] = "pgup";
      map[Key::PageDown] = "pgdn";
      map[Key::End] = "end";
      map[Key::Home] = "home";
      map[Key::Left] = "left";
      map[Key::Up] = "up";
      map[Key::Right] = "right";
      map[Key::Down] = "down";
      map[Key::Delete] = "del",
      map[Key::Mul] = "*";
      map[Key::Add] = "+";
      map[Key::Sub] = "-";
      map[Key::Dot] = ".";
      map[Key::FwdSlash] = "/";
      map[Key::Semicolon] = ";";
      map[Key::Comma] = ",";
      map[Key::Question] = "?";
      map[Key::Tilda] = "~";
      map[Key::LeftCurly] = "{";
      map[Key::Bar] = "|";
      map[Key::RightCurly] = "}";
      map[Key::Quote] = "\"";
      map[Key::MinusUnderLine] = "-";
      map[Key::PlusEq] = "=";
      //map[Key::F1 = 112, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
      map[Key::LeftShift] = "lshift";
      map[Key::RightShift] = "rshift";
      map[Key::LeftCtrl] = "lctrl";
      map[Key::RightCtrl] = "rctrl";
      for(int i = 0; i < 10; ++i) {
        std::string num = std::to_string(i);
        map[static_cast<Key>(static_cast<int>(Key::Key0) + i)] = num;
        map[static_cast<Key>(static_cast<int>(Key::Num0) + i)] = "num" + num;
      }
      for(char i = 0; i < 26; ++i) {
        map[static_cast<Key>(static_cast<int>(Key::KeyA) + i)] = std::string(static_cast<size_t>(1), 'a' + i);
      }
      for(char i = 0; i < 24; ++i) {
        map[static_cast<Key>(static_cast<int>(Key::F24) + i)] = "f" + std::string(static_cast<size_t>(1), '1' + i);
      }
      return map;
  }();
  const std::unordered_map<std::string, Key> STRING_TO_KEY = []() {
    std::unordered_map<std::string, Key> result;
    result.reserve(KEY_TO_STRING.size());
    for(const auto pair : KEY_TO_STRING)
      result[pair.second] = pair.first;
    return result;
  }();

  Key _charToKey(char c, char baseChar, Key baseKey) {
    uint8_t offset = static_cast<uint8_t>(c - baseChar);
    return static_cast<Key>(static_cast<uint8_t>(baseKey) + offset);
  }

  using namespace Engine;
  void tickInit(SystemContext<EntityFactory>& context) {
    context.get<EntityFactory>().createEntityWithComponents<RawInputBufferComponent, RawInputComponent>();
  }

  void _tickKeyStates(RawInputComponent& input) {
    input.mWheelDelta = 0;
    input.mMouseDelta = Syx::Vec2(0);
    for(auto it = input.mKeyStates.begin(); it != input.mKeyStates.end();) {
      switch(it->second) {
        case KeyState::Up:
        case KeyState::Released:
          it = input.mKeyStates.erase(it);
          break;

        case KeyState::Triggered:
        default:
          it->second = KeyState::Down;
          ++it;
          break;
      }
    }
  }

  void onKeyStateChange(RawInputComponent& input, Key key, KeyState state) {
    //Update the state in the map, removing (or preventing creation) if the state is up, since absence from the map implies up
    auto it = input.mKeyStates.find(key);
    if(it != input.mKeyStates.end()) {
      if(state != KeyState::Up) {
        it->second = state;
      }
      else {
        input.mKeyStates.erase(it);
      }
    }
    else if(state != KeyState::Up) {
      input.mKeyStates[key] = state;
    }
  }

  void onRawInputEvent(const RawInputEvent::KeyEvent& e, RawInputComponent& input) {
    onKeyStateChange(input, e.mKey, e.mState);
  }

  void onRawInputEvent(const RawInputEvent::MouseKeyEvent& e, RawInputComponent& input) {
    onKeyStateChange(input, e.mKey, e.mState);
    input.mMousePos = e.mPos;
  }

  void onRawInputEvent(const RawInputEvent::MouseMoveEvent& e, RawInputComponent& input) {
    input.mMousePos = e.mPos;
    input.mMouseDelta = e.mDelta;
  }

  void onRawInputEvent(const RawInputEvent::MouseWheelEvent& e, RawInputComponent& input) {
    input.mWheelDelta = e.mAmount;
  }

  using UpdateView = View<Write<RawInputBufferComponent>, Write<RawInputComponent>>;
  void tickUpdate(SystemContext<UpdateView>& context) {
    for(auto entity : context.get<UpdateView>()) {
      std::vector<RawInputEvent>& events = entity.get<RawInputBufferComponent>().mEvents;
      RawInputComponent& input = entity.get<RawInputComponent>();

      //Update deltas
      _tickKeyStates(input);

      //Process new events
      for(const RawInputEvent& event : events) {
        std::visit([&input](const auto& e) {
          onRawInputEvent(e, input);
        }, event.mData);
      }
      events.clear();
    }
  }
}

KeyState RawInputSystem::getAsciiState(const RawInputComponent& input, char c) {
  switch(c) {
    case '!': return _shiftAnd(input, Key::Key1);
    case '@': return _shiftAnd(input, Key::Key2);
    case '#': return _shiftAnd(input, Key::Key3);
    case '$': return _shiftAnd(input, Key::Key4);
    case '%': return _shiftAnd(input, Key::Key5);
    case '^': return _shiftAnd(input, Key::Key6);
    case '&': return _shiftAnd(input, Key::Key7);
    case '*': return _shiftAnd(input, Key::Key8);
    case '(': return _shiftAnd(input, Key::Key9);
    case ')': return _shiftAnd(input, Key::Key0);
    case '-': return _noShift(input, Key::MinusUnderLine);
    case '_': return _shiftAnd(input, Key::MinusUnderLine);
    case '+': return _shiftAnd(input, Key::PlusEq);
    case '=': return _noShift(input, Key::PlusEq);
    case '[': return _noShift(input, Key::LeftCurly);
    case '{': return _shiftAnd(input, Key::LeftCurly);
    case ']': return _noShift(input, Key::RightCurly);
    case '}': return _shiftAnd(input, Key::RightCurly);
    case '\\': return _noShift(input, Key::Bar);
    case '|': return _shiftAnd(input, Key::Bar);
    case ';': return _noShift(input, Key::Semicolon);
    case ':': return _shiftAnd(input, Key::Semicolon);
    case '\'': return _noShift(input, Key::Quote);
    case '"': return _shiftAnd(input, Key::Quote);
    case ',': return _noShift(input, Key::Comma);
    case '<': return _shiftAnd(input, Key::Comma);
    case '.': return _noShift(input, Key::Dot);
    case '>': return _shiftAnd(input, Key::Dot);
    case '/': return _noShift(input, Key::Question);
    case '?': return _shiftAnd(input, Key::Question);
    case '`': return _noShift(input, Key::Tilda);
    case '~': return _shiftAnd(input, Key::Tilda);
    case ' ': return _or(_noShift(input, Key::Space), _shiftAnd(input, Key::Space));
  }

  // Key codes numbers and characters match
  if(c >= 'a' && c <= 'z')
    return _noShift(input, _charToKey(c, 'a', Key::KeyA));
  if(c >= 'A' && c <= 'Z')
    return _shiftAnd(input, _charToKey(c, 'A', Key::KeyA));
  if(c >= '0' && c <= '9')
    return _noShift(input, _charToKey(c, '0', Key::Key0));
  return KeyState::Up;
}

std::shared_ptr<Engine::System> RawInputSystem::init() {
  return ecx::makeSystem("InitInput", &tickInit);
}

std::shared_ptr<Engine::System> RawInputSystem::update() {
  return ecx::makeSystem("UpdateInput", &tickUpdate);
}
