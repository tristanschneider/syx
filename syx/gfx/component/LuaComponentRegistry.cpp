#include "Precompile.h"
#include "component/LuaComponentRegistry.h"

#include "component/Component.h"

void LuaComponentRegistry::registerComponent(const std::string& name, Constructor constructor) {
  mNameToInfo[name] = CompInfo{ constructor, constructor(0) };
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
