#include "Precompile.h"
#include "lua/LuaGameContext.h"

#include "asset/LuaScript.h"
#include "component/Component.h"
#include "component/LuaComponentRegistry.h"
#include <lua.hpp>
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "LuaGameObject.h"
#include "Space.h"
#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"
#include "Util.h"

#include "event/BaseComponentEvents.h"
#include "provider/GameObjectHandleProvider.h"

namespace {
  const char* LUA_CONTEXT_KEY = "LuaGameContext";
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

  Handle getHandle() const override {
    return mObj;
  }

  const Component* addComponent(const char* componentName) {
    return mGameContext.addComponent(componentName, _get());
  }

  const Component* getComponent(const ComponentType& type) {
    return _get().getComponent(type);
  }

  const Component* getComponentByPropName(const char* name) const {
    return _get().getComponent(name);
  }

  void forEachComponent(const std::function<void(const Component&)>& callback) const {
    return _get().forEachComponent(callback);
  }

  const Component* addComponentFromPropName(const char* name) override {
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
};


class LuaGameContext : public ILuaGameContext {
public:
  virtual ~LuaGameContext() = default;
  LuaGameContext(LuaGameSystem& system, std::unique_ptr<Lua::State> state)
    : mSystem(system)
    , mState(std::move(state)) {
    //Store a reference to this state's game context
    lua_pushlightuserdata(*mState, this);
    lua_setfield(*mState, LUA_REGISTRYINDEX, LUA_CONTEXT_KEY);
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

  virtual void update(float dt) {
    mComponentCache.clear();
    mObjectCache.clear();

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
          std::shared_ptr<Asset> script = mSystem.getAssetRepo().getAsset(AssetInfo(comp.getScript()));
          //If script isn't done loading, wait until later
          if(!script || script->getState() != AssetState::Loaded) {
            return;
          }

          //Load the script on to the top of the stack
          const std::string& scriptSource = static_cast<LuaScript&>(*script).get();
          {
            Lua::StackAssert sa(*mState);
            if(int loadError = luaL_loadstring(*mState, scriptSource.c_str())) {
              //TODO: better error reporting
              printf("Error loading script %s: %s\n", static_cast<LuaScript&>(*script).getInfo().mUri.c_str(), lua_tostring(*mState, -1));
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

  virtual const Component* addComponentFromPropName(const char* name, const LuaGameObject& owner) override {
    const Component* component = mSystem.getComponentRegistry().getInstanceByPropName(name);
    return component ? addComponent(component->getTypeInfo().mTypeName, owner) : nullptr;
  }

  virtual const Component* addComponent(const std::string& name, const LuaGameObject& owner) override {
    if(std::optional<ComponentType> typeID = mSystem.getComponentRegistry().getComponentFullType(name)) {
      if(const Component* existing = owner.getComponent(*typeID)) {
        return existing;
      }

      std::unique_ptr<Component> newComponent = mSystem.getComponentRegistry().construct(name, owner.getHandle());
      //TODO: it doesn't seem like this is needed, but also it could replace the global one stored in lua state
      newComponent->setSystem(mSystem);

      // TODO: component publisher should somehow be updated to do this
      mSystem.getMessageQueue().get().push(AddComponentEvent(owner.getHandle(), newComponent->getType()));
      Component* result = newComponent.get();
      mComponentCache.insert({ owner.getHandle(), *typeID }, std::move(newComponent));
      return result;
    }
    return nullptr;
  }

  virtual void removeComponentFromPropName(const char* name, Handle owner) override {
    if(const Component* component = mSystem.getComponentRegistry().getInstanceByPropName(name)) {
      removeComponent(component->getTypeInfo().mTypeName, owner);
    }
  }

  virtual void removeComponent(const std::string& name, Handle owner) override {
    if(std::optional<ComponentType> typeID = mSystem.getComponentRegistry().getComponentFullType(name)) {
      mSystem.getMessageQueue().get().push(RemoveComponentEvent(owner, *typeID));
      mComponentCache.insertDeletionMarker({ owner, *typeID });
    }
  }

  virtual IGameObject& addGameObject() override {
    const Handle newHandle = mSystem.getGameObjectGen().newHandle();
    mSystem.getMessageQueue().get().push(AddGameObjectEvent(newHandle));
    mObjectCache.insert(newHandle, std::make_unique<LuaGameObject>(newHandle));

    //Can't be null because we just put it in the object cache
    return *getGameObject(newHandle);
  }

  virtual void removeGameObject(Handle object) override {
    mSystem.getMessageQueue().get().push(RemoveGameObjectEvent(object));
    mObjectCache.insertDeletionMarker(object);
    if(auto it = mBoundObjects.find(object); it != mBoundObjects.end()) {
      mBoundObjects.erase(it);
    }
  }

  virtual IGameObject* getGameObject(Handle object) {
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

  virtual MessageQueueProvider& getMessageProvider() const override {
    return mSystem.getMessageQueueProvider();
  }

  virtual lua_State* getLuaState() override {
    return mState->get();
  }

  virtual AssetRepo& getAssetRepo() override {
    return mSystem.getAssetRepo();
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

  virtual std::unique_ptr<Lua::State> createLuaState() const override {
    auto state = std::make_unique<Lua::State>();
    mSystem._openAllLibs(state->get());
    return state;
  }

  virtual SystemProvider& getSystemProvider() override {
    return mSystem.getSystemProvider();
  }

  virtual const LuaComponentRegistry& getComponentRegistry() const override {
    return mSystem.getComponentRegistry();
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

  HandleMap<std::weak_ptr<LuaGameObject>> mObjects;
  std::unordered_map<Handle, LuaBoundGameObject> mBoundObjects;
  std::unique_ptr<Lua::State> mState;
  //Cache of components used to hold changes made this frame until next frame when they are officially applied via messages.
  //For this reason, the cache is cleared every frame
  ObjectCache<ComponentKey, Component> mComponentCache;
  ObjectCache<Handle, LuaGameObject> mObjectCache;
  LuaGameSystem& mSystem;
};

namespace Lua {
  std::unique_ptr<ILuaGameContext> createGameContext(LuaGameSystem& system, std::unique_ptr<Lua::State> state) {
    return std::make_unique<LuaGameContext>(system, std::move(state));
  }

  ILuaGameContext& checkGameContext(lua_State* l) {
    return LuaGameContext::_getContext(l);
  }
}