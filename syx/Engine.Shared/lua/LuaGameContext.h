#pragma once

class AssetRepo;
class Component;
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
  class State;
}

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

  //Functions intended to be used from lua
  virtual const Component* addComponentFromPropName(const char* name, const LuaGameObject& owner) = 0;
  virtual const Component* addComponent(const std::string& name, const LuaGameObject& owner) = 0;
  virtual void removeComponentFromPropName(const char* name, Handle owner) = 0;
  virtual void removeComponent(const std::string& name, Handle owner) = 0;
  virtual IGameObject& addGameObject() = 0;
  virtual void removeGameObject(Handle object) = 0;

  virtual lua_State* getLuaState() = 0;
  virtual IGameObject* getGameObject(Handle object) = 0;
  virtual MessageQueueProvider& getMessageProvider() const = 0;

  virtual AssetRepo& getAssetRepo() = 0;
  virtual const HandleMap<std::shared_ptr<LuaGameObject>>& getObjects() const = 0;
  virtual const ProjectLocator& getProjectLocator() const = 0;
  virtual GameObjectHandleProvider& getGameObjectGen() = 0;
  virtual const Space& getOrCreateSpace(Handle id) const = 0;
  virtual IWorkerPool& getWorkerPool() = 0;
  virtual std::unique_ptr<Lua::State> createLuaState() const = 0;
  virtual SystemProvider& getSystemProvider() = 0;
  virtual const LuaComponentRegistry& getComponentRegistry() const = 0;
};

namespace Lua {
  std::unique_ptr<ILuaGameContext> createGameContext(LuaGameSystem& system, std::unique_ptr<Lua::State> state);
  ILuaGameContext& checkGameContext(lua_State* l);
}
