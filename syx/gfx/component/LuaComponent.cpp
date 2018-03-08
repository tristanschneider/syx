#include "Precompile.h"
#include "component/LuaComponent.h"

DEFINE_COMPONENT(LuaComponent) {
  mScript = 0;
}

size_t LuaComponent::getScript() const {
  return mScript;
}

void LuaComponent::setScript(size_t script) {
  mScript = script;
}
