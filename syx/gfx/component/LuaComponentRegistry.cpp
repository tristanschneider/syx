#include "Precompile.h"
#include "component/LuaComponentRegistry.h"

#include "component/Component.h"

void LuaComponentRegistry::registerComponent(const std::string& name, Constructor constructor) {
  mNameToCtor[name] = constructor;
}

std::unique_ptr<Component> LuaComponentRegistry::construct(const std::string& name, Handle owner) const {
  auto it = mNameToCtor.find(name);
  if(it != mNameToCtor.end())
    return std::move(it->second(owner));
  return nullptr;
}