#include "Precompile.h"
#include "Gameobject.h"
#include "component/Component.h"

Gameobject::Gameobject(Handle handle)
  : mHandle(handle)
  , mTransform(handle) {
}

Gameobject::~Gameobject() {
}

void Gameobject::init() {
  for(auto& comp : mComponents.getBuffer())
    comp->init();
}

void Gameobject::update(float dt) {
  for(auto& comp : mComponents.getBuffer())
    comp->update(dt);
}

void Gameobject::uninit() {
  for(auto& comp : mComponents.getBuffer())
    comp->uninit();
}

Handle Gameobject::getHandle() {
  return mHandle;
}

Component& Gameobject::addComponent(std::unique_ptr<Component> component) {
  Handle h = component->getHandle();
  return *mComponents.pushBack(std::move(component), h);
}

void Gameobject::removeComponent(Handle handle) {
  mComponents.erase(handle);
}

Component* Gameobject::getComponent(Handle handle) {
  switch(static_cast<ComponentType>(handle)) {
    case ComponentType::Transform: return &mTransform;
    default: {
      std::unique_ptr<Component>* found = mComponents.get(handle);
      return found ? found->get() : nullptr;
    }
  }
}

Component* Gameobject::getComponent(ComponentType type) {
  return getComponent(static_cast<Handle>(type));
}

