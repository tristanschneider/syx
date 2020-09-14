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

  int Cache::push(lua_State* l, void* data, size_t handle, const char* typeOverride) const {
    Lua::StackAssert sa(l, 1);
    _pushCache(l);

    //Try to get this object from the cache
    lua_pushinteger(l, static_cast<lua_Integer>(handle));
    lua_gettable(l, -2);
    //If object wasn't found in the cache, create a new one, store it in the cache, and return it
    if(lua_isnil(l, -1)) {
      //Pop off the null
      lua_pop(l, 1);
      void** newObj = static_cast<void**>(lua_newuserdata(l, sizeof(void*)));
      *newObj = data;
      luaL_setmetatable(l, typeOverride ? typeOverride : mUserdataType.c_str());
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

  int Cache::invalidate(lua_State* l, size_t handle, const char* typeOverride) const {
    Lua::StackAssert sa(l);
    _pushCache(l);

    //Try to get this object from the cache
    lua_pushinteger(l, static_cast<lua_Integer>(handle));
    lua_gettable(l, -2);
    if(!lua_isnil(l, -1)) {
      //Null object in cache
      void** cachedObj = static_cast<void**>(luaL_checkudata(l, -1, typeOverride ? typeOverride : mUserdataType.c_str()));
      *cachedObj = nullptr;
      //Remove object from registry
      lua_pushinteger(l, static_cast<lua_Integer>(handle));
      lua_pushnil(l);
      lua_settable(l, -4);
    }
    //else: object wasn't in cache, which is fine, nothing to invalidate
    //Pop cache entry and cache table
    lua_pop(l, 2);
    return 0;
  }

  void* Cache::getParam(lua_State* l, int index, const char* typeOverride) const {
    return *static_cast<void**>(luaL_checkudata(l, index, typeOverride ? typeOverride : mUserdataType.c_str()));
  }

  void* Cache::checkParam(lua_State* l, int index, const char* typeOverride) const {
    void* result = getParam(l, index, typeOverride);
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


  ScopedCacheInstance::~ScopedCacheInstance() {
    if(auto factory = mFactory.lock()) {
      factory->_destroyInstance(&mState, mHandle);
    }
  }

  std::unique_ptr<ScopedCacheEntry> ScopedCacheInstance::createEntry(void* data, std::string type) const {
    if(auto factory = mFactory.lock()) {
      return factory->_createEntry(&mState, data, std::move(type), mHandle);
    }
    return nullptr;
  }

  ScopedCacheEntry::~ScopedCacheEntry() {
    if(auto factory = mFactory.lock()) {
      factory->_destroyEntry(&mState, mHandle, mInstance, mType.c_str());
    }
  }

  //Push reference to object from the cache that can be popped with lua_pop. The object on the stack doesn't need to be scoped because this may be used from lua
  int ScopedCacheEntry::push() const {
    if(auto factory = mFactory.lock()) {
      return factory->_pushEntry(&mState, mHandle, mInstance);
    }
    return 0;
  }

  void* ScopedCacheEntry::getParam(lua_State* l, int index, const char* typeOverride) {
    return *static_cast<void**>(luaL_checkudata(l, index, typeOverride));
  }

  void* ScopedCacheEntry::checkParam(lua_State* l, int index, const char* typeOverride) {
    void* result = getParam(l, index, typeOverride);
    luaL_argcheck(l, result != nullptr, index, "object is invalid");
    return result;
  }

  ScopedCacheFactory::ScopedCacheFactory(const std::string& key)
    : mInstanceKey(std::hash<std::string>()(key))
    , mEntryKey(0) {
  }

  std::unique_ptr<ScopedCacheInstance> ScopedCacheFactory::createInstance(lua_State* l) {
    StackAssert sa(l);
    const size_t key = mInstanceKey.fetch_add(1);
    lua_pushinteger(l, static_cast<lua_Integer>(key));
    lua_newtable(l);
    lua_settable(l, LUA_REGISTRYINDEX);
    return std::make_unique<ScopedCacheInstance>(shared_from_this(), key, *l);
  }

  void ScopedCacheFactory::_destroyInstance(lua_State* l, size_t key) const {
    StackAssert sa(l);
    lua_pushinteger(l, static_cast<lua_Integer>(key));
    lua_pushnil(l);
    lua_settable(l, LUA_REGISTRYINDEX);
  }

  std::unique_ptr<ScopedCacheEntry> ScopedCacheFactory::_createEntry(lua_State* l, void* data, std::string type, size_t instanceKey) {
    Lua::StackAssert sa(l);
    _pushCache(l, instanceKey);

    const size_t newKey = mEntryKey.fetch_add(1);
    //Try to get this object from the cache
    lua_pushinteger(l, static_cast<lua_Integer>(newKey));
    lua_gettable(l, -2);
    //The entry shouldn't already exist. If it does that probably means a different object has interfered with the lua state
    assert(lua_isnil(l, -1) && "Entry already exists");
    if(lua_isnil(l, -1)) {
      //Pop off the null
      lua_pop(l, 1);
      void** newObj = static_cast<void**>(lua_newuserdata(l, sizeof(void*)));
      *newObj = data;
      luaL_setmetatable(l, type.c_str());
      //Store the new userdata in the cache
      lua_pushinteger(l, static_cast<lua_Integer>(newKey));
      lua_pushvalue(l, -2);
      lua_settable(l, -4);
    }
    //else cached gameobject is on top of stack
    //Remove cache table
    lua_pop(l, 2);
    return std::make_unique<ScopedCacheEntry>(weak_from_this(), newKey, std::move(type), *l, instanceKey);
  }

  int ScopedCacheFactory::_destroyEntry(lua_State* l, size_t entryHandle, size_t instanceHandle, const char* type) const {
    Lua::StackAssert sa(l);
    _pushCache(l, instanceHandle);

    //Try to get this object from the cache
    lua_pushinteger(l, static_cast<lua_Integer>(entryHandle));
    lua_gettable(l, -2);
    assert(!lua_isnil(l, -1) && "Cache entry didn't exist but should have since it's owned by ScopedEntry");
    if(!lua_isnil(l, -1)) {
      //Null object in cache
      void** cachedObj = static_cast<void**>(luaL_checkudata(l, -1, type));
      *cachedObj = nullptr;
      //Remove object from registry
      lua_pushinteger(l, static_cast<lua_Integer>(entryHandle));
      lua_pushnil(l);
      lua_settable(l, -4);
    }
    //Pop cache entry and cache table
    lua_pop(l, 2);
    return 0;
  }

  int ScopedCacheFactory::_pushEntry(lua_State* l, size_t entryHandle, size_t instanceHandle) const {
    Lua::StackAssert sa(l, 1);
    _pushCache(l, instanceHandle);

    //Try to get this object from the cache
    lua_pushinteger(l, static_cast<lua_Integer>(entryHandle));
    lua_gettable(l, -2);
    assert(!lua_isnil(l, -1) && "Entry should exist");
    //Remove the cache itself
    lua_remove(l, -2);
    return 1;
  }

  void ScopedCacheFactory::_pushCache(lua_State* l, size_t key) const {
    lua_pushinteger(l, static_cast<lua_Integer>(key));
    lua_gettable(l, LUA_REGISTRYINDEX);
  }
}