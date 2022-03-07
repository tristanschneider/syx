#pragma once
#include "event/Event.h"
#include "SyxVec2.h"
#include "ecs/component/RawInputComponent.h"

struct KeyEvent : public TypedEvent<KeyEvent> {
  KeyEvent() = default;
  KeyEvent(Key key, KeyState state)
    : mKey(key)
    , mState(state) {
  }

  Key mKey = Key::Count;
  KeyState mState = KeyState::Invalid;
};

struct MouseKeyEvent : public TypedEvent<MouseKeyEvent> {
  MouseKeyEvent() = default;
  MouseKeyEvent(Key key, KeyState state, const Syx::Vec2& pos)
    : mKey(key)
    , mState(state)
    , mPos(pos) {
  }

  Key mKey = Key::Count;
  KeyState mState = KeyState::Invalid;
  Syx::Vec2 mPos;
};

struct MouseMoveEvent : public TypedEvent<MouseMoveEvent> {
  MouseMoveEvent() = default;
  MouseMoveEvent(const Syx::Vec2& pos, const Syx::Vec2& delta)
    : mPos(pos)
    , mDelta(delta) {
  }

  Syx::Vec2 mPos;
  Syx::Vec2 mDelta;
};

struct MouseWheelEvent : public TypedEvent<MouseWheelEvent> {
  MouseWheelEvent(float amount = 0.f)
    : mAmount(amount) {
  }

  float mAmount = 0;
};