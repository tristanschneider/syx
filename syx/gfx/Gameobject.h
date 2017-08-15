#pragma once
#include "MappedBuffer.h"
#include "Component.h"
#include "components/Transform.h"

enum class ComponentType : uint8_t;

class Gameobject {
public:
  ~Gameobject();
  Gameobject(Handle handle = InvalidHandle);
  Gameobject(Gameobject&&) = default;
  Gameobject& operator=(Gameobject&&) = default;

  void init();
  void update(float dt);
  void uninit();

  Handle getHandle();

  Component& addComponent(std::unique_ptr<Component> component);
  void removeComponent(Handle handle);
  Component* getComponent(Handle handle);
  Component* getComponent(ComponentType type);

  template<typename CompType>
  CompType* getComponent(Handle handle) {
    return static_cast<CompType*>(getComponent(handle));
  }
  template<typename CompType>
  CompType* getComponent(ComponentType handle) {
    return static_cast<CompType*>(getComponent(handle));
  }

private:
  Handle mHandle;
  MappedBuffer<std::unique_ptr<Component>> mComponents;
  //Hard code transform because everything needs it
  Transform mTransform;
};