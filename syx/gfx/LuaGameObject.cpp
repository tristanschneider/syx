#include "Precompile.h"
#include "LuaGameObject.h"

LuaGameObject::LuaGameObject(Handle h)
  : mHandle(h)
  , mTransform(h) {
}

Handle LuaGameObject::getHandle() const {
  return mHandle;
}

void LuaGameObject::addComponent(std::unique_ptr<Component> component) {
  mComponents[component->getType()] = std::move(component);
}

void LuaGameObject::removeComponent(size_t type) {
  mComponents[type] = nullptr;
}

Component* LuaGameObject::getComponent(size_t type) {
  return mComponents[type].get();
}

Transform& LuaGameObject::getTransform() {
  return mTransform;
}
