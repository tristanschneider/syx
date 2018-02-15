#pragma once
#include "MappedBuffer.h"
#include "component/Component.h"
#include "component/Transform.h"

class Gameobject {
public:
  ~Gameobject();
  Gameobject(Handle handle = InvalidHandle, MessageQueueProvider* messaging = nullptr);
  Gameobject(Gameobject&&) = default;
  Gameobject& operator=(Gameobject&&) = default;

  void init();
  void update(float dt);
  void uninit();

  Handle getHandle();

  Component& addComponent(std::unique_ptr<Component> component);
  void removeComponent(Handle handle);
  Component* getComponent(size_t type);

  template<typename CompType>
  CompType* getComponent() {
    return static_cast<CompType*>(getComponent(Component::typeId<CompType>()));
  }

  Transform& getTransform() {
    return mTransform;
  }

private:
  Handle mHandle;
  HandleMap<std::unique_ptr<Component>> mComponents;
  //Hard code transform because everything needs it
  Transform mTransform;
  MessageQueueProvider* mMessaging;
};