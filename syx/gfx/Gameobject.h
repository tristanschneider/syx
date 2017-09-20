#pragma once
#include "MappedBuffer.h"
#include "component/Component.h"
#include "component/Transform.h"

enum class ComponentType : uint8_t;
class MessagingSystem;

class Gameobject {
public:
  ~Gameobject();
  Gameobject(Handle handle = InvalidHandle, MessagingSystem* messaging = nullptr);
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

  Transform& getTransform() {
    return mTransform;
  }

private:
  Handle mHandle;
  HandleMap<std::unique_ptr<Component>> mComponents;
  //Hard code transform because everything needs it
  Transform mTransform;
  MessagingSystem* mMessaging;
};