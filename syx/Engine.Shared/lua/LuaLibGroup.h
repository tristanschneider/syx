#pragma once

struct lua_State;

namespace Lua {
  class LuaLibGroup {
  public:
    virtual ~LuaLibGroup() {}
    virtual void open(lua_State* l) = 0;
  };
}