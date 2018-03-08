#pragma once
#include "Component.h"

class LuaComponent : public Component {
public:
  LuaComponent(Handle owner);

  size_t getScript() const;
  void setScript(size_t script);

private:
  size_t mScript;
};