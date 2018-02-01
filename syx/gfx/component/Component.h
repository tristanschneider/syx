#pragma once

class Gameobject;
class MessageQueueProvider;

enum class ComponentType : uint8_t {
  Transform,
  Graphics,
  Physics
};

class Component {
public:
  Component(Handle type, Handle owner, MessageQueueProvider* messaging);
  ~Component();

  //Should always be overriden, but needs to have empty implementation to avoid linker error with getHandle call in constructor
  virtual Handle getHandle() const {
    return mType;
  }
  virtual void init() {}
  virtual void update(float dt) {}
  virtual void uninit() {}

  Handle getOwner() {
    return mOwner;
  }

protected:
  Handle mOwner;
  MessageQueueProvider* mMessaging;
  Handle mType;
};