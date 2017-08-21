#pragma once

class Gameobject;
class MessagingSystem;

enum class ComponentType : uint8_t {
  Transform,
  Graphics,
  Physics
};

class Component {
public:
  Component(Handle owner, MessagingSystem* messaging)
    : mOwner(owner)
    , mMessaging(messaging) {
  }

  virtual Handle getHandle() const = 0;
  virtual void init() {}
  virtual void update(float dt) {}
  virtual void uninit() {}

  Handle getOwner() {
    return mOwner;
  }

protected:
  Handle mOwner;
  MessagingSystem* mMessaging;
};