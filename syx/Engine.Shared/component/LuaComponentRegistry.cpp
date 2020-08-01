#include "Precompile.h"
#include "component/LuaComponentRegistry.h"

#include "component/Component.h"
#include "Util.h"

void LuaComponentRegistry::registerComponent(const std::string& name, Constructor constructor) {
  auto& info = mNameToInfo[name] = CompInfo{ constructor, constructor(0) };
  mPropNameToInfo[info.instance->getTypeInfo().mPropNameConstHash] = &info;
}

std::unique_ptr<Component> LuaComponentRegistry::construct(const std::string& name, Handle owner) const {
  auto it = mNameToInfo.find(name);
  if(it != mNameToInfo.end())
    return std::move(it->second.constructor(owner));
  return nullptr;
}

std::optional<size_t> LuaComponentRegistry::getComponentType(const std::string& name) const {
  auto it = mNameToInfo.find(name);
  return it == mNameToInfo.end() ? std::nullopt : std::make_optional(it->second.instance->getType());
}

std::optional<ComponentType> LuaComponentRegistry::getComponentFullType(const std::string& name) const {
  auto it = mNameToInfo.find(name);
  return it == mNameToInfo.end() ? std::nullopt : std::make_optional(it->second.instance->getFullType());
}

const Component* LuaComponentRegistry::getInstanceByPropName(const char* name) const {
  return getInstanceByPropNameConstHash(Util::constHash(name));
}

const Component* LuaComponentRegistry::getInstanceByPropNameConstHash(size_t hash) const {
  auto it = mPropNameToInfo.find(hash);
  return it != mPropNameToInfo.end() ? it->second->instance.get() : nullptr;
}

void LuaComponentRegistry::forEachComponent(const std::function<void(const Component&)>& callback) const {
  for(const auto& it : mNameToInfo) {
    callback(*it.second.instance);
  }
}
