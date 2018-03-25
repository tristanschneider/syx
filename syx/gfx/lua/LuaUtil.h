#pragma once
struct lua_State;
struct luaL_Reg;

namespace Lua {
  class State;

  namespace Util {
    using CFunc = int(*)(lua_State*);

    void printTop(State& state);
    void printGlobal(State& state, const std::string& global);
    void registerClass(lua_State* l, const luaL_Reg* statics, const luaL_Reg* members, const char* className);
    int intIndexOverload(lua_State* l, CFunc overload);
    int intNewIndexOverload(lua_State* l, CFunc overload);
  }
}