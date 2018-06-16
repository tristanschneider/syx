#include "Precompile.h"
#include "component/LuaComponentRegistry.h"

#include "component/Component.h"

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

bool LuaComponentRegistry::getComponentType(const std::string& name, size_t& type) const {
  auto it = mNameToInfo.find(name);
  if(it != mNameToInfo.end()) {
    type = it->second.instance->getType();
    return true;
  }
  return false;
}

const Component* LuaComponentRegistry::getInstanceByPropName(const char* name) const {
  return getInstanceByPropNameConstHash(Util::constHash(name));
}

const Component* LuaComponentRegistry::getInstanceByPropNameConstHash(size_t hash) const {
  auto it = mPropNameToInfo.find(hash);
  return it != mPropNameToInfo.end() ? it->second->instance.get() : nullptr;
}
