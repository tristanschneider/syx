#pragma once
#include <atomic>

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
    int push(lua_State* l, void* data, size_t handle, const char* typeOverride = nullptr) const;
    //Invalidate the given object in the cache. References to this object in lua may keep it alive, but the internal pointer will be null, which can be checked through getParam != nullptr
    int invalidate(lua_State* l, size_t handle, const char* typeOverride = nullptr) const;
    //Get the cached userdata type from the stack. Will return null if object has been invalidated
    void* getParam(lua_State* l, int index, const char* typeOverride = nullptr) const;
    //Get the cached userdata type and throw a lua error if it is not valid. Guaranteed to return non-null pointer if it returns
    void* checkParam(lua_State* l, int index, const char* typeOverride = nullptr) const;

    //Generate a new handle to associate with userdata. This is threadsafe
    size_t nextHandle();

  private:
    void _pushCache(lua_State* l) const;

    size_t mKey;
    std::atomic<size_t> mNextHandle;
    std::string mUserdataType;
  };

  class ScopedCacheFactory;

  class ScopedCacheEntry {
  public:
    ScopedCacheEntry(std::weak_ptr<ScopedCacheFactory> factory, size_t handle, std::string type, lua_State& state, size_t instance)
      : mFactory(std::move(factory))
      , mHandle(handle)
      , mType(std::move(type))
      , mState(state)
      , mInstance(instance) {
    }
    ~ScopedCacheEntry();
    ScopedCacheEntry(ScopedCacheEntry&) = delete;
    ScopedCacheEntry(ScopedCacheEntry&&) = delete;
    ScopedCacheEntry& operator=(const ScopedCacheEntry&) = delete;
    ScopedCacheEntry& operator=(ScopedCacheEntry&&) = delete;

    //Push reference to object from the cache that can be popped with lua_pop. The object on the stack doesn't need to be scoped because this may be used from lua
    int push() const;

    //Get the cached userdata type from the stack. Will return null if object has been invalidated
    static void* getParam(lua_State* l, int index, const char* typeOverride);
    //Get the cached userdata type and throw a lua error if it is not valid. Guaranteed to return non-null pointer if it returns
    static void* checkParam(lua_State* l, int index, const char* typeOverride);

  private:
    std::weak_ptr<ScopedCacheFactory> mFactory;
    size_t mHandle = 0;
    size_t mInstance = 0;
    std::string mType;
    lua_State& mState;
  };

  class ScopedCacheInstance {
  public:
    ScopedCacheInstance(std::weak_ptr<ScopedCacheFactory> factory, size_t handle, lua_State& state)
      : mFactory(std::move(factory))
      , mHandle(handle)
      , mState(state) {
    }
    ~ScopedCacheInstance();
    ScopedCacheInstance(ScopedCacheInstance&) = delete;
    ScopedCacheInstance(ScopedCacheInstance&&) = delete;
    ScopedCacheInstance& operator=(const ScopedCacheInstance&) = delete;
    ScopedCacheInstance& operator=(ScopedCacheInstance&&) = delete;

    //Push userdata of the given handle onto the stack
    std::unique_ptr<ScopedCacheEntry> createEntry(void* data, std::string type) const;

  private:
    std::weak_ptr<ScopedCacheFactory> mFactory;
    size_t mHandle = 0;
    lua_State& mState;
  };

  class ScopedCacheFactory : public std::enable_shared_from_this<ScopedCacheFactory> {
  public:
    friend class ScopedCacheInstance;
    friend class ScopedCacheEntry;

    //Unique key to identify this cache in the c registry.
    ScopedCacheFactory(const std::string& key);

    std::unique_ptr<ScopedCacheInstance> createInstance(lua_State* l);

  private:
    void _destroyInstance(lua_State* l, size_t key) const;

    std::unique_ptr<ScopedCacheEntry> _createEntry(lua_State* l, void* data, std::string type, size_t instanceKey);
    //Push userdata of the given handle onto the stack, using the cached version if available. Otherwise new userdata is created and cached.
    int _pushEntry(lua_State* l, size_t entryHandle, size_t instanceHandle) const;
    //Invalidate the given object in the cache. References to this object in lua may keep it alive, but the internal pointer will be null, which can be checked through getParam != nullptr
    int _destroyEntry(lua_State* l, size_t entryHandle, size_t instanceHandle, const char* type) const;
    void _pushCache(lua_State* l, size_t key) const;

    std::atomic<size_t> mInstanceKey;
    std::atomic<size_t> mEntryKey;
    std::string mUserdataType;
  };
}