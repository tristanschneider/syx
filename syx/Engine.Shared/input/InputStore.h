#pragma once
#include "event/EventHandler.h"

class EventHandler;
struct FrameStart;
struct KeyEvent;
enum class Key : uint8_t;
enum class KeyState : uint8_t;
struct MouseKeyEvent;
struct MouseMoveEvent;
struct MouseWheelEvent;

//Listens to input events and stores the most recent state for users that prefer to query input rather than use the events directly
//It is expected to be used by the one responsible for event handling and as such is not thread safe if used on a different thread during event handling
class InputStore : public EventListener, public std::enable_shared_from_this<InputStore> {
public:
  InputStore() = default;

  void init(EventHandler& handler);

  KeyState getKeyState(const std::string& key) const;
  KeyState getKeyState(Key key) const;
  bool getKeyDown(Key key) const;
  bool getKeyDownOrTriggered(Key key) const;
  bool getKeyUp(Key key) const;
  bool getKeyTriggered(Key key) const;
  bool getKeyReleased(Key key) const;
  KeyState getAsciiState(char c) const;
  //Get mouse information in pixels
  Syx::Vec2 getMousePos() const;
  Syx::Vec2 getMouseDelta() const;
  float getWheelDelta() const;

private:
  void onKeyStateChange(Key key, KeyState state);
  void onKeyEvent(const KeyEvent& e);
  void onMouseKeyEvent(const MouseKeyEvent& e);
  void onMouseMoveEvent(const MouseMoveEvent& e);
  void onMouseWheelEvent(const MouseWheelEvent& e);
  void onFrameStart(const FrameStart&);

  KeyState _shiftAnd(Key key) const;
  KeyState _noShift(Key key) const;
  KeyState _or(KeyState a, KeyState b) const;
  void _registerEventHandlers(EventHandler& handler);

  //All key states with abscence implying released
  std::unordered_map<Key, KeyState> mKeyStates;
  Syx::Vec2 mMousePos;
  Syx::Vec2 mMouseDelta;
  float mWheelDelta = 0.f;
};