#pragma once
#include "Util.h"

class AssetRepo;
class Component;
class ComponentRegistryProvider;
struct ComponentType;
class GameObjectHandleProvider;
struct IGameObject;
class IWorkerPool;
class LuaComponentRegistry;
class LuaGameObject;
class LuaGameSystem;
class LuaLibGroup;
struct lua_State;
class MessageQueueProvider;
class ProjectLocator;
class Space;
class SystemProvider;

namespace Lua {
  struct IState;
  class ScopedCacheEntry;
  class ScopedCacheInstance;
}

namespace FileSystem {
  struct IFileSystem;
}

//Wraps access to the underlying object so that it can either be backed by the actual object owned by LuaGameSystem, or the pending context-local state owned by ILuaGameContext
struct IComponent {
  //Get the underlying component. Ideally all accessors could be abstracted away so the underlying component is entirely hidden, but that would make for a very cumbersome interface
  virtual const Component& get() const = 0;
  template<class T>
  const T& get() const {
    return static_cast<const T&>(get());
  }
  //Overwrite the component with new values. The intended use is getting the component above, making a copy, modifying, and setting
  virtual void set(const Component& newValue) = 0;

  virtual Lua::ScopedCacheEntry& getOrCreateCacheEntry() = 0;
};

struct IGameObject {
  virtual ~IGameObject() = default;
  virtual Handle getHandle() const = 0;
  virtual IComponent* addComponent(const char* componentName) = 0;
  virtual IComponent* getComponent(const ComponentType& type) = 0;
  virtual IComponent* getComponentByPropName(const char* name) = 0;
  virtual Lua::ScopedCacheEntry& getOrCreateCacheEntry() = 0;
  //Intentionally accessing the underlying object to avoid needing to create a bunch of extra IComponents when probably not all of them are needed
  virtual void forEachComponent(const std::function<void(const Component&)>& callback) const = 0;
  virtual IComponent* addComponentFromPropName(const char* name) = 0;
  virtual void removeComponentFromPropName(const char* name) = 0;
  virtual void removeComponent(const std::string& name) = 0;
};

//A game context is a worker for a subset of gameobjects owned by LuaGameSystem. They are intended to load
//balance the work of running scripts across multiple threads, where each context is only used on one thread
//at a time.
struct ILuaGameContext {
  virtual ~ILuaGameContext() = default;

  //Add or remove an object. That means this context is responsible for updating the given object.
  //All objects are still accessible for interaction via the LuaGameSystem
  virtual void addObject(std::weak_ptr<LuaGameObject> object) = 0;
  virtual void clear() = 0;
  virtual void update(float dt) = 0;

  //Clear local cache of object state. Not needed for contexts owned by LuaGameSystem, but needed if using them by hand to ensure state outside of the context is fetched when it changes
  virtual void clearCache() = 0;

  //Functions intended to be used from lua
  virtual IComponent* addComponentFromPropName(const char* name, const LuaGameObject& owner) = 0;
  virtual IComponent* addComponent(const std::string& name, const LuaGameObject& owner) = 0;
  virtual void removeComponentFromPropName(const char* name, Handle owner) = 0;
  virtual void removeComponent(const std::string& name, Handle owner) = 0;
  virtual IGameObject& addGameObject() = 0;
  virtual void removeGameObject(Handle object) = 0;

  virtual lua_State* getLuaState() = 0;
  virtual IGameObject* getGameObject(Handle object) = 0;
  virtual IComponent* getComponent(Handle object, const ComponentType& component) = 0;
  virtual const Component* getRawComponent(Handle object, const ComponentType& component) = 0;
  virtual MessageQueueProvider& getMessageProvider() const = 0;

  virtual Lua::ScopedCacheInstance& getGameobjectCache() = 0;
  virtual Lua::ScopedCacheInstance& getComponentCache() = 0;

  virtual AssetRepo& getAssetRepo() = 0;
  virtual const HandleMap<std::shared_ptr<LuaGameObject>>& getObjects() const = 0;
  virtual const ProjectLocator& getProjectLocator() const = 0;
  virtual GameObjectHandleProvider& getGameObjectGen() = 0;
  virtual const Space& getOrCreateSpace(Handle id) const = 0;
  virtual IWorkerPool& getWorkerPool() = 0;
  virtual std::unique_ptr<Lua::IState> createLuaState() = 0;
  virtual SystemProvider& getSystemProvider() = 0;
  virtual const ComponentRegistryProvider& getComponentRegistry() const = 0;
  virtual FileSystem::IFileSystem& getFileSystem() = 0;
};

namespace Lua {
  std::unique_ptr<ILuaGameContext> createGameContext(LuaGameSystem& system);
  ILuaGameContext& checkGameContext(lua_State* l);
}
