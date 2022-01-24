#pragma once

#include "lua.hpp"
#include "lua/LuaStackAssert.h"
#include "TypeInfo.h"

//Uses ecx::TypeInfo to provide LuaTypeInfo<T> which can be used to read or write to lua
//Types can specialize to expose their lua functionality
namespace Lua {
  template<class T, class Enabled = void>
  struct LuaTypeInfo {
    static_assert(sizeof(T) == -1, "Should have specialized");
    static int push(lua_State* l, const T& value);

    static T fromTop(lua_State* l);
  };

  template<>
  struct LuaTypeInfo<bool> {
    static int push(lua_State* l, bool value) {
      lua_pushboolean(l, value);
      return 1;
    }

    static bool fromTop(lua_State* l) {
      return lua_toboolean(l, -1);
    }
  };

  template<>
  struct LuaTypeInfo<std::string> {
    static int push(lua_State* l, const std::string& value) {
      lua_pushstring(l, value.c_str());
      return 1;
    }

    static std::string fromTop(lua_State* l) {
      const char* result = lua_tostring(l, -1);
      return result ? std::string(result) : std::string();
    }
  };

  template<class T>
  struct LuaTypeInfo<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static int push(lua_State* l, const T& value) {
      lua_pushnumber(l, static_cast<lua_Number>(value));
      return 1;
    }

    static T fromTop(lua_State* l) {
      return static_cast<T>(lua_tonumber(l, -1));
    }
  };

  template<class T>
  struct LuaTypeInfo<T, std::enable_if_t<std::is_integral_v<T>>> {
    static int push(lua_State* l, const T& value) {
      lua_pushinteger(l, static_cast<lua_Integer>(value));
      return 1;
    }

    static T fromTop(lua_State* l) {
      return static_cast<T>(lua_tointeger(l, -1));
    }
  };

  template<class T>
  struct LuaTypeInfo<T, std::enable_if_t<(ecx::StaticTypeInfo<T>::MemberCount > 0)>> {
    static int push(lua_State* l, const T& value) {
      Lua::StackAssert a(l, 1);
      lua_newtable(l);

      ecx::StaticTypeInfo<T>::visitShallow([l](const std::string& name, const auto& value) {
        //Field name
        lua_pushstring(l, name.c_str());
        //Push value for field by recursing into the LuaTypeInfo for this type
        LuaTypeInfo<std::decay_t<decltype(value)>>::push(l, value);
        //Put the key/value into the table
        lua_settable(l, -3);
      }, value);
      return 1;
    }

    static T fromTop(lua_State* l) {
      Lua::StackAssert a(l);
      T result;
      ecx::StaticTypeInfo<T>::visitShallow([l](const std::string& name, auto& value) {
        //If the value was found, get the type info for this member and use it to read the value
        if(lua_getfield(l, -1, name.c_str()) != LUA_TNONE) {
          value = LuaTypeInfo<std::decay_t<decltype(value)>>::fromTop(l);
          lua_pop(l, 1);
        }
      }, result);

      return result;
    }
  };
};