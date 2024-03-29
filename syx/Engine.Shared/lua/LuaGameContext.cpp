#include "Precompile.h"
#include "lua/LuaGameContext.h"

#include "asset/ImmediateAssetWrapper.h"
#include "asset/LuaScript.h"
#include "component/Component.h"
#include "component/ComponentPublisher.h"
#include "component/LuaComponentRegistry.h"
#include <lua.hpp>
#include "lua/LuaCache.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "LuaGameObject.h"
#include "registry/IDRegistry.h"
#include "Space.h"
#include "system/LuaGameSystem.h"
#include "Util.h"

#include "event/BaseComponentEvents.h"
#include "provider/GameObjectHandleProvider.h"

namespace {
  const char* LUA_CONTEXT_KEY = "LuaGameContext";

  struct LuaStateWithCache : public Lua::IState {
    LuaStateWithCache()
      //The name given is arbitrary, just something unlikely to be used by lua scripts to avoid name collisions
      : mState(std::make_unique<Lua::State>())
      , mGameobjectFactory(std::make_shared<Lua::ScopedCacheFactory>("_goc_gameobject"))
      , mComponentFactory(std::make_shared<Lua::ScopedCacheFactory>("_goc_component"))
      , mGameobjectInstance(mGameobjectFactory->createInstance(*mState))
      , mComponentInstance(mComponentFactory->createInstance(*mState))
    {
    }

    operator lua_State*() override {
      return *mState;
    }

    std::unique_ptr<Lua::State> mState;
    std::shared_ptr<Lua::ScopedCacheFactory> mGameobjectFactory;
    std::shared_ptr<Lua::ScopedCacheFactory> mComponentFactory;

    std::shared_ptr<Lua::ScopedCacheInstance> mGameobjectInstance;
    std::shared_ptr<Lua::ScopedCacheInstance> mComponentInstance;
  };
}

struct ComponentKey {
  Handle mOwner = 0;
  ComponentType mType;

  size_t operator()() const {
    return Util::hashCombine(std::hash<Handle>()(mOwner), mType());
  }

  bool operator==(const ComponentKey& rhs) const {
    return mOwner == rhs.mOwner 
      && mType == rhs.mType;
  }

  bool operator!=(const ComponentKey& rhs) const {
    return !(*this == rhs);
  }
};

namespace std {
  template<>
  struct hash<ComponentKey> {
    std::size_t operator()(const ComponentKey& key) const noexcept {
      return key();
    }
  };
}

template<class Key, class Value>
class ObjectCache {
public:
  std::optional<Value*> tryGet(const Key& key) {
    if(auto it = mCachedComponents.find(key); it != mCachedComponents.end()) {
      return std::make_optional(it->second.get());
    }
    return std::nullopt;
  }

  std::optional<const Value*> tryGet(const Key& key) const {
    if(auto it = mCachedComponents.find(key); it != mCachedComponents.end()) {
      return std::make_optional(it->second.get());
    }
    return std::nullopt;
  }

  void insert(const Key& key, std::unique_ptr<Value> component) {
    mCachedComponents[key] = std::move(component);
  }

  void insertDeletionMarker(const Key& key) {
    mCachedComponents[key] = nullptr;
  }

  void clear() {
    mCachedComponents.clear();
  }

  auto begin() {
    return mCachedComponents.begin();
  }

  auto end() {
    return mCachedComponents.end();
  }

private:
  std::unordered_map<Key, std::unique_ptr<Value>> mCachedComponents;
};

//This is intended to be used by the scripting layer to act as if objects are mutable, but in reality it's just using temporary state on ILuaGameContext
//and sending messages so that all actual state mutations are done through messages processed on the next frame.
class LuaBoundGameObject : public IGameObject {
public:
  LuaBoundGameObject(Handle handle, ILuaGameContext& gameContext, std::function<const LuaGameObject&(Handle)> getObj)
    : mObj(handle)
    , mGameContext(gameContext)
    , mGetObj(std::move(getObj)) {
  }

  Handle getRuntimeID() const override {
    return mObj;
  }

  const UniqueID& getUniqueID() const override {
    return _get().getUniqueID();
  }

  IComponent* addComponent(const char* componentName) override {
    return mGameContext.addComponent(componentName, _get());
  }

  IComponent* getComponent(const ComponentType& type) override {
    return mGameContext.getComponent(mObj, type);
  }

  IComponent* getComponentByPropName(const char* name) override {
    const Component* instance = mGameContext.getComponentRegistry().getReader().first.getInstanceByPropName(name);
    return instance ? mGameContext.getComponent(mObj, instance->getFullType()) : nullptr;
  }

  Lua::ScopedCacheEntry& getOrCreateCacheEntry() override {
    if(!mCacheEntry) {
      mCacheEntry = mGameContext.getGameobjectCache().createEntry(this, LuaGameObject::CLASS_NAME);
    }
    return *mCacheEntry;
  }

  void forEachComponent(const std::function<void(const Component&)>& callback) const override {
    _get().forEachComponent(callback);
  }

  IComponent* addComponentFromPropName(const char* name) override {
    return mGameContext.addComponentFromPropName(name, _get());
  }

  void removeComponentFromPropName(const char* name) override {
    mGameContext.removeComponentFromPropName(name, mObj);
  }

  void removeComponent(const std::string& name) override {
    mGameContext.removeComponent(name, mObj);
  }

private:
  const LuaGameObject& _get() const {
    return mGetObj(mObj);
  }

  const Handle mObj;
  ILuaGameContext& mGameContext;
  std::function<const LuaGameObject&(Handle)> mGetObj;
  std::unique_ptr<Lua::ScopedCacheEntry> mCacheEntry;
};

class LuaBoundComponent : public IComponent {
public:
  LuaBoundComponent(Handle object, const ComponentType& component, ILuaGameContext& context, std::function<Component&()> getMutableComponent)
    : mObject(object)
    , mComponent(component)
    , mContext(context)
    , mGetMutableComponent(std::move(getMutableComponent)) {
  }

  virtual const Component& get() const override {
    const Component* result = mContext.getRawComponent(mObject, mComponent);
    assert(result && "Component should exist or this object should have been destroyed");
    return *result;
  }

  virtual void set(const Component& newValue) override {
    assert(newValue.getOwner() == mObject && "Component setter should not be used to change ownership");
    Component& self = mGetMutableComponent();
    ComponentPublisher publisher(self);
    //Publish the changes to persist the change
    publisher.publish(newValue, mContext.getMessageProvider());
    //Perform the modification now on the local object so context-local state is consistent
    self.set(newValue);
  }

  virtual Lua::ScopedCacheEntry& getOrCreateCacheEntry() override {
    if(!mCacheEntry) {
      mCacheEntry = mContext.getComponentCache().createEntry(this, get().getTypeInfo().mTypeName);
    }
    return *mCacheEntry;
  }

private:
  const Handle mObject;
  const ComponentType mComponent;
  ILuaGameContext& mContext;
  std::function<Component&()> mGetMutableComponent;
  std::unique_ptr<Lua::ScopedCacheEntry> mCacheEntry;
};

class LuaGameContext : public ILuaGameContext {
public:
  virtual ~LuaGameContext() = default;
  LuaGameContext(LuaGameSystem& system)
    : mSystem(system) {
    mState = _createLuaState();
  }

  void _storeContextInState(lua_State* l) {
    //Store a reference to this state's game context
    lua_pushlightuserdata(l, this);
    lua_setfield(l, LUA_REGISTRYINDEX, LUA_CONTEXT_KEY);
  }

  static LuaGameContext& _getContext(lua_State* l) {
    Lua::StackAssert sa(l);
    lua_getfield(l, LUA_REGISTRYINDEX, LUA_CONTEXT_KEY);
    LuaGameContext* context = static_cast<LuaGameContext*>(lua_touserdata(l, -1));
    if(!context) {
      luaL_error(l, "LuaGameContext instance didn't exist");
    }
    lua_pop(l, 1);
    return *context;
  }

  virtual void addObject(std::weak_ptr<LuaGameObject> object) {
    if(auto obj = object.lock()) {
      mObjects[obj->getHandle()] = object;
    }
  }

  virtual void clear() {
    mObjects.clear();
  }

  virtual void clearCache() {
    //This is needed to ensure that state is fetched from the game system when needed. If rebuilding these every frame proves to be too costly they could be cached longer and kept in sync via an observer.
    //Bound objects only need to be deleted if the object is, it's not a problem if state on the object changed
    mComponentCache.clear();
    mObjectCache.clear();
    mBoundObjects.clear();
    mBoundComponents.clear();
  }

  virtual void update(float dt) {
    clearCache();

    Lua::StackAssert sa(*mState);
    for(auto& objIt : mObjects) {
      std::shared_ptr<LuaGameObject> obj = objIt.second.lock();
      IGameObject* boundObj = getGameObject(objIt.first);
      if(!obj || !boundObj) {
        continue;
      }

      LuaGameObject::push(*mState, *boundObj);
      int selfIndex = lua_gettop(*mState);
      dt *= mSystem.getSpace(obj->getSpace()).getTimescale();
      const bool doUpdate = dt != 0;

      obj->forEachLuaComponent([this, selfIndex, doUpdate, dt](LuaComponent& comp) {
        //If the component needs initialization, get the script and initialize it
        if(comp.needsInit()) {
          if(!comp.mScriptAsset) {
            comp.mScriptAsset = ImmediateAsset::create(AssetInfo(comp.getScript()), *getMessageProvider().getMessageQueue(), typeId<LuaGameSystem, System>());
          }
          const auto* script = comp.mScriptAsset ? comp.mScriptAsset->cast<LuaScript>() : nullptr;
          //If script isn't done loading, wait until later
          if(!script || script->getState() != AssetState::Loaded) {
            return;
          }

          //Load the script on to the top of the stack
          const std::string& scriptSource = script->get();
          {
            Lua::StackAssert sa(*mState);
            if(int loadError = luaL_loadstring(*mState, scriptSource.c_str())) {
              //TODO: better error reporting
              printf("Error loading script %s: %s\n", script->getInfo().mUri.c_str(), lua_tostring(*mState, -1));
            }
            else {
              comp.init(*mState, selfIndex);
            }
            //Pop off the error or the script
            lua_pop(*mState, 1);
          }
        }
        //Else sandbox is already initialized, do the update
        else if(doUpdate) {
          comp.update(*mState, dt, selfIndex);
        }
      });
      //pop gameobject
      lua_pop(*mState, 1);
    }
  }

  virtual IComponent* addComponentFromPropName(const char* name, const LuaGameObject& owner) override {
    const Component* component = mSystem.getComponentRegistry().getReader().first.getInstanceByPropName(name);
    return component ? addComponent(component->getTypeInfo().mTypeName, owner) : nullptr;
  }

  virtual IComponent* addComponent(const std::string& name, const LuaGameObject& owner) override {
    if(std::optional<ComponentType> typeID = mSystem.getComponentRegistry().getReader().first.getComponentFullType(name)) {
      //If the component already exists, return the bound form of it
      if(const Component* existing = owner.getComponent(*typeID)) {
        return getComponent(owner.getHandle(), existing->getFullType());
      }

      //The component doesn't already exist, make a new one
      std::unique_ptr<Component> newComponent = mSystem.getComponentRegistry().getReader().first.construct(name, owner.getHandle());
      //TODO: it doesn't seem like this is needed, but also it could replace the global one stored in lua state
      newComponent->setSystem(mSystem);

      //TODO: component publisher should somehow be updated to do this
      mSystem.getMessageQueue().get().push(AddComponentEvent(owner.getHandle(), newComponent->getFullType()));
      Component* result = newComponent.get();
      mComponentCache.insert({ owner.getHandle(), *typeID }, std::move(newComponent));
      //Now that it's been added to the component cache, use the getComponent code path to create and return the bound object
      return getComponent(owner.getHandle(), result->getFullType());
    }
    return nullptr;
  }

  virtual void removeComponentFromPropName(const char* name, Handle owner) override {
    if(const Component* component = mSystem.getComponentRegistry().getReader().first.getInstanceByPropName(name)) {
      removeComponent(component->getTypeInfo().mTypeName, owner);
    }
  }

  virtual void removeComponent(const std::string& name, Handle owner) override {
    if(std::optional<ComponentType> typeID = mSystem.getComponentRegistry().getReader().first.getComponentFullType(name)) {
      mSystem.getMessageQueue().get().push(RemoveComponentEvent(owner, *typeID));
      mComponentCache.insertDeletionMarker({ owner, *typeID });
    }
  }

  //Usually components are looked up on demand, starting with the caches in this content and looking in the game system if this doesn't have it.
  //Built in components are implicitly created by this context when the object is, so they need to be manually added to the caches here otherwise
  //the lookup won't see them until they have been propagated to the game system
  IGameObject* _cacheBuiltInComponents(Handle handle) {
    if(IGameObject* boundObject = getGameObject(handle)) {
      boundObject->forEachComponent([this, handle](const Component& comp) {
        std::unique_ptr<Component> newComponent = comp.clone();

        Component* result = newComponent.get();
        mComponentCache.insert({ handle, result->getFullType() }, std::move(newComponent));
      });
      return boundObject;
    }
    return nullptr;
  }

  virtual IGameObject& addGameObject() override {
    const Handle newHandle = mSystem.getGameObjectGen().newHandle();
    std::shared_ptr<IClaimedUniqueID> uniqueID = mSystem.getIDRegistry().generateNewUniqueID();
    mSystem.getMessageQueue().get().push(AddGameObjectEvent(newHandle, uniqueID));
    mObjectCache.insert(newHandle, std::make_unique<LuaGameObject>(newHandle, std::move(uniqueID)));

    //Can't be null because we just put it in the object cache
    return *_cacheBuiltInComponents(newHandle);
  }

  virtual IGameObject* tryAddGameObject(const UniqueID& uniqueID) override {
    if(std::shared_ptr<IClaimedUniqueID> claimedID = mSystem.getIDRegistry().tryClaimKnownID(uniqueID)) {
      const Handle newHandle = mSystem.getGameObjectGen().newHandle();
      mSystem.getMessageQueue().get().push(AddGameObjectEvent(newHandle, claimedID));
      mObjectCache.insert(newHandle, std::make_unique<LuaGameObject>(newHandle, claimedID));

      return _cacheBuiltInComponents(newHandle);
    }
    return nullptr;
  }

  virtual void removeGameObject(Handle object) override {
    mSystem.getMessageQueue().get().push(RemoveGameObjectEvent(object));
    mObjectCache.insertDeletionMarker(object);
    if(auto it = mBoundObjects.find(object); it != mBoundObjects.end()) {
      mBoundObjects.erase(it);
    }
  }

  virtual IGameObject* getGameObject(Handle object) override {
    auto it = mBoundObjects.find(object);
    if(it == mBoundObjects.end()) {
      //If the object exists, create the bound object and return it
      if (_getObject(object)) {
        auto getObj = [this](Handle handle) -> const LuaGameObject& { return *_getObject(handle); };
        return &mBoundObjects.emplace(std::make_pair(object, LuaBoundGameObject(object, *this, std::move(getObj)))).first->second;
      }
      //Object doesn't exist so there's nothing to bind to
      return nullptr;
    }
    //Object already bound, return that
    return &it->second;
  }

  virtual IComponent* getComponent(Handle object, const ComponentType& component) override {
    const ComponentKey key{ object, component };
    auto it = mBoundComponents.find(key);
    if(it == mBoundComponents.end()) {
      //If the object exists, create the bound object and return it
      if (_getComponent(object, component)) {
        auto getMutableComponent = [this, key]() -> Component& {
          //This needs to be mutable meaning it must return one of the cached objects representing context-local state
          if(auto existing = mComponentCache.tryGet(key); existing && *existing) {
            return **existing;
          }

          //No cached component exists, make one from the real version
          const Component* component = _getComponentFromSystem(key);
          assert(component && "Component should exist if bound object representing it still does");
          std::unique_ptr<Component> mutableCopy = component->clone();
          Component& result = *mutableCopy;
          mComponentCache.insert(key, std::move(mutableCopy));
          return result;
        };

        return &mBoundComponents.emplace(std::make_pair(key, LuaBoundComponent(object, component, *this, std::move(getMutableComponent)))).first->second;
      }
      //Object doesn't exist so there's nothing to bind to
      return nullptr;
    }
    //Object already bound, return that
    return &it->second;
  }

  virtual const Component* getRawComponent(Handle object, const ComponentType& component) override {
    return _getComponent(object, component);
  }

  virtual MessageQueueProvider& getMessageProvider() const override {
    return mSystem.getMessageQueueProvider();
  }

  virtual Lua::ScopedCacheInstance& getGameobjectCache() override {
    return *mState->mGameobjectInstance;
  }

  virtual Lua::ScopedCacheInstance& getComponentCache() override {
    return *mState->mComponentInstance;
  }

  virtual lua_State* getLuaState() override {
    return *mState;
  }

  virtual const HandleMap<std::shared_ptr<LuaGameObject>>& getObjects() const override {
    return mSystem.getObjects();
  }

  virtual const ProjectLocator& getProjectLocator() const override {
    return mSystem.getProjectLocator();
  }

  virtual GameObjectHandleProvider& getGameObjectGen() override {
    return mSystem.getGameObjectGen();
  }

  virtual const Space& getOrCreateSpace(Handle id) const override {
    return mSystem.getSpace(id);
  }

  virtual IWorkerPool& getWorkerPool() override {
    return mSystem.getWorkerPool();
  }

  virtual std::unique_ptr<Lua::IState> createLuaState() override {
    return _createLuaState();
  }

  std::unique_ptr<LuaStateWithCache> _createLuaState() {
    auto state = std::make_unique<LuaStateWithCache>();
    mSystem._openAllLibs(*state);
    _storeContextInState(*state);
    return state;
  }

  virtual const ComponentRegistryProvider& getComponentRegistry() const override {
    return mSystem.getComponentRegistry();
  }

  virtual FileSystem::IFileSystem& getFileSystem() override {
    return mSystem.getFileSystem();
  }

  virtual IIDRegistry& getIDRegistry() override {
    return mSystem.getIDRegistry();
  }

  const InputStore& getInput() const override {
    return mSystem.getInput();
  }

private:
  const LuaGameObject* _getObject(Handle handle) {
    //Present local pending state as the truth within this context
    if(auto cached = mObjectCache.tryGet(handle)) {
      return *cached;
    }
    //Not necessary to look in owned object list since the system has them all
    return mSystem.getObject(handle);
  }

  const Component* _getComponentFromSystem(const ComponentKey& key) {
    const LuaGameObject* obj = mSystem.getObject(key.mOwner);
    return obj ? obj->getComponent(key.mType) : nullptr;
  }

  const Component* _getComponent(Handle handle, const ComponentType& component) {
    if(auto cached = mComponentCache.tryGet({ handle, component })) {
      return *cached;
    }
    //Could do _getObject or go straight to the system. Since the component is not in the component cache it wouldn't make any sense for the object itself to be in the object cache
    return _getComponentFromSystem({ handle, component });
  }

  HandleMap<std::weak_ptr<LuaGameObject>> mObjects;
  std::unordered_map<Handle, LuaBoundGameObject> mBoundObjects;
  std::unordered_map<ComponentKey, LuaBoundComponent> mBoundComponents;
  std::unique_ptr<LuaStateWithCache> mState;
  //Cache of components used to hold changes made this frame until next frame when they are officially applied via messages.
  //For this reason, the cache is cleared every frame
  ObjectCache<ComponentKey, Component> mComponentCache;
  ObjectCache<Handle, LuaGameObject> mObjectCache;
  LuaGameSystem& mSystem;
};

namespace Lua {
  std::unique_ptr<ILuaGameContext> createGameContext(LuaGameSystem& system) {
    return std::make_unique<LuaGameContext>(system);
  }

  ILuaGameContext& checkGameContext(lua_State* l) {
    return LuaGameContext::_getContext(l);
  }
}