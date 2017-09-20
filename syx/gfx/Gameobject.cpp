#include "Precompile.h"
#include "Gameobject.h"
#include "component/Component.h"

Gameobject::Gameobject(Handle handle, MessagingSystem* messaging)
  : mHandle(handle)
  , mTransform(handle, messaging) {
}

Gameobject::~Gameobject() {
}

void Gameobject::init() {
  //Fire a transform update on init so any components added before this have an up to date transform
  mTransform.set(mTransform.get(), true);
}

void Gameobject::update(float dt) {
}

void Gameobject::uninit() {
}

Handle Gameobject::getHandle() {
  return mHandle;
}

Component& Gameobject::addComponent(std::unique_ptr<Component> component) {
  Handle h = component->getHandle();
  assert(mComponents.find(h) == mComponents.end()); //Duplicate component added
  Component* result = component.get();
  mComponents[h] = std::move(component);
  return *result;
}

void Gameobject::removeComponent(Handle handle) {
  mComponents.erase(handle);
}

Component* Gameobject::getComponent(Handle handle) {
  switch(static_cast<ComponentType>(handle)) {
    case ComponentType::Transform: return &mTransform;
    default: {
      auto it = mComponents.find(handle);
      return it != mComponents.end() ? it->second.get() : nullptr;
    }
  }
}

Component* Gameobject::getComponent(ComponentType type) {
  return getComponent(static_cast<Handle>(type));
}

