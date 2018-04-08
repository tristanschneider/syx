#include "Precompile.h"
#include "lua/LuaCache.h"

#include "lua/LuaStackAssert.h"
#include <lua.hpp>

namespace Lua {
  Cache::Cache(const std::string& key, const std::string& userdataType)
    : mKey(std::hash<std::string>()(key))
    , mUserdataType(userdataType)
    , mNextHandle(0) {
  }

  void Cache::createCache(lua_State* l) const {
    StackAssert sa(l);
    lua_pushinteger(l, static_cast<lua_Integer>(mKey));
    lua_newtable(l);
    lua_settable(l, LUA_REGISTRYINDEX);
  }

  void Cache::destroyCache(lua_State* l) const {
    StackAssert sa(l);
    lua_pushinteger(l, static_cast<lua_Integer>(mKey));
    lua_pushnil(l);
    lua_settable(l, LUA_REGISTRYINDEX);
  }

  int Cache::push(lua_State* l, void* data, size_t handle) const {
    Lua::StackAssert sa(l, 1);
    _pushCache(l);

    //Try to get this object from the cache
    lua_pushinteger(l, static_cast<lua_Integer>(handle));
    lua_gettable(l, -2);
    //If object wasn't found in the cache, create a new one, store it in the cache, and return it
    if(lua_isnil(l, -1)) {
      void** newObj = static_cast<void**>(lua_newuserdata(l, sizeof(void*)));
      *newObj = data;
      luaL_setmetatable(l, mUserdataType.c_str());
      //Store the new userdata in the cache
      lua_pushinteger(l, static_cast<lua_Integer>(handle));
      lua_pushvalue(l, -2);
      lua_settable(l, -4);
    }
    //else cached gameobject is on top of stack
    //Remove cache table
    lua_remove(l, -2);
    return 1;
  }

  int Cache::invalidate(lua_State* l, size_t handle) const {
    Lua::StackAssert sa(l);
    _pushCache(l);

    //Try to get this object from the cache
    lua_pushinteger(l, static_cast<lua_Integer>(handle));
    lua_gettable(l, -2);
    if(!lua_isnil(l, -1)) {
      //Null object in cache
      void** cachedObj = static_cast<void**>(luaL_checkudata(l, -1, mUserdataType.c_str()));
      *cachedObj = nullptr;
      //Remove object from registry
      lua_pushinteger(l, static_cast<lua_Integer>(handle));
      lua_pushnil(l);
      lua_settable(l, -3);
    }
    //else: object wasn't in cache, which is fine, nothing to invalidate
    //Pop cache table
    lua_pop(l, 1);
    return 0;
  }

  void* Cache::getParam(lua_State* l, int index) const {
    return *static_cast<void**>(luaL_checkudata(l, index, mUserdataType.c_str()));
  }

  void* Cache::checkParam(lua_State* l, int index) const {
    void* result = getParam(l, index);
    luaL_argcheck(l, result != nullptr, index, "object is invalid");
    return result;
  }

  size_t Cache::nextHandle() {
    return mNextHandle.fetch_add(1);
  }

  void Cache::_pushCache(lua_State* l) const {
    lua_pushinteger(l, static_cast<lua_Integer>(mKey));
    lua_gettable(l, LUA_REGISTRYINDEX);
  }
}