#pragma once

class Gameobject;

enum class ComponentType : uint8_t {
  Transform,
  Graphics
};

class Component {
public:
  Component(Handle owner)
    : mOwner(owner) {
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
};