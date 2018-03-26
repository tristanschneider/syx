#include "Precompile.h"
#include "lua/LuaUtil.h"

#include <lua.hpp>
#include "lua/LuaSerializer.h"
#include "lua/LuaState.h"
#include "lua/LuaStackAssert.h"

namespace Lua {
  namespace Util {
    void printTop(State& state) {
      Lua::Serializer s("\t", "\n", 1);
      std::string buff;
      s.serializeTop(state, buff);
      printf("%s\n", buff.c_str());
    }

    void printGlobal(State& state, const std::string& global) {
      Lua::Serializer s("\t", "\n", 1);
      std::string buff;
      s.serializeGlobal(state, global, buff);
      printf("%s\n", buff.c_str());
    }

    void registerClass(lua_State* l, const luaL_Reg* statics, const luaL_Reg* members, const char* className, bool defaultIndex, bool defaultNewIndex) {
      Lua::StackAssert sa(l);
      luaL_newmetatable(l, className);
      luaL_setfuncs(l, members, 0);
      if(defaultIndex) {
        lua_pushvalue(l, -1);
        lua_setfield(l, -2, "__index");
      }
      if(defaultNewIndex) {
        lua_pushvalue(l, -1);
        lua_setfield(l, -2, "__newindex");
      }
      lua_pop(l, 1);

      luaL_newlib(l, statics);
      lua_setglobal(l, className);
    }

    int intIndexOverload(lua_State* l, CFunc overload) {
      if(lua_isinteger(l, 2))
        return overload(l);

      lua_getmetatable(l, 1);
      //Push the __index key
      lua_pushvalue(l, 2);
      //get metatable[key], which is the function user is presumably trying to index
      lua_gettable(l, -2);
      //remove metatable, leaving function on top
      lua_remove(l, 3);
      return 1;
    }

    int intNewIndexOverload(lua_State* l, CFunc overload) {
      if(lua_isinteger(l, 2))
        return overload(l);

      lua_getmetatable(l, 1);
      //Push key
      lua_pushvalue(l, 2);
      //Push value
      lua_pushvalue(l, 3);
      //array.metatable[key] = value
      lua_settable(l, -3);
      return 0;
    }
  }
}