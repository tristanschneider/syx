#pragma once

struct lua_State;

namespace Lua {
  //Caches userdata in C registry so references can be created without requiring
  //allocations each time. It also allows cached objects to be invalidated,
  //making it a safe way to store raw pointers in userdata.
  //This doesn't own a lua state as it's safe to use it across multiple lua states
  //In this case the userdata can store its handle, which will apply to all caches containing it
  class Cache {
  public:
    //Unique key to identify this cache in the c registry.
    Cache(const std::string& key, const std::string& userdataType);

    //Create/destroy the cache table in this lua state
    void createCache(lua_State* l) const;
    void destroyCache(lua_State* l) const;

    //Push userdata of the given handle onto the stack, using the cached version if available. Otherwise new userdata is created and cached.
    int push(lua_State* l, void* data, size_t handle) const;
    //Invalidate the given object in the cache. References to this object in lua may keep it alive, but the internal pointer will be null, which can be checked through getParam != nullptr
    int invalidate(lua_State* l, size_t handle) const;
    //Get the cached userdata type from the stack. Will return null if object has been invalidated
    void* getParam(lua_State* l, int index) const;
    //Get the cached userdata type and throw a lua error if it is not valid. Guaranteed to return non-null pointer if it returns
    void* checkParam(lua_State* l, int index) const;

    //Generate a new handle to associate with userdata. This is threadsafe
    size_t nextHandle();

  private:
    void _pushCache(lua_State* l) const;

    size_t mKey;
    std::atomic<size_t> mNextHandle;
    std::string mUserdataType;
  };
}