#include "Precompile.h"
#include "LuaGameObject.h"

#include "component/LuaComponent.h"

LuaGameObject::LuaGameObject(Handle h)
  : mHandle(h)
  , mTransform(h) {
}

LuaGameObject::~LuaGameObject() {
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

LuaComponent* LuaGameObject::addLuaComponent(size_t script) {
  auto it = mLuaComponents.emplace(script, mHandle);
  LuaComponent& result = it.first->second;
  result.setScript(script);
  return &result;
}

LuaComponent* LuaGameObject::getLuaComponent(size_t script) {
  auto it = mLuaComponents.find(script);
  return it != mLuaComponents.end() ? &it->second : nullptr;
}

void LuaGameObject::removeLuaComponent(size_t script) {
  auto it = mLuaComponents.find(script);
  if(it != mLuaComponents.end())
    mLuaComponents.erase(it);
}

std::unordered_map<size_t, LuaComponent>& LuaGameObject::getLuaComponents() {
  return mLuaComponents;
}
