#pragma once
#include "MappedBuffer.h"
#include "Component.h"
#include "components/TransformComponent.h"

class Gameobject {
public:
  ~Gameobject();
  Gameobject(Handle handle = InvalidHandle);
  Gameobject(Gameobject&&) = default;
  Gameobject& operator=(Gameobject&&) = default;

  void init();
  void update(float dt);
  void uninit();
  Component& addComponent(std::unique_ptr<Component> component);
  void removeComponent(Handle handle);
  Component* getComponent(Handle handle);

private:
  Handle mHandle;
  MappedBuffer<std::unique_ptr<Component>> mComponents;
  //Hard code transform because everything needs it
  TransformComponent mTransform;
};