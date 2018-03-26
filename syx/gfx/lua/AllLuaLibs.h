#pragma once
#include "lua/LuaLibGroup.h"

namespace Lua {
  class AllLuaLibs : public LuaLibGroup {
  public:
    void open(lua_State* l) override;
  };
}