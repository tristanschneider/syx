#pragma once

class Component;
class LuaGameObject;
class LuaGameSystem;
struct lua_State;
class MessageQueueProvider;

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
  virtual Component* addComponentFromPropName(const char* name, LuaGameObject& owner) = 0;
  virtual Component* addComponent(const std::string& name, LuaGameObject& owner) = 0;
  virtual void removeComponentFromPropName(const char* name, Handle owner) = 0;
  virtual void removeComponent(const std::string& name, Handle owner) = 0;
  virtual LuaGameObject& addGameObject() = 0;
  virtual void removeGameObject(Handle object) = 0;

  virtual MessageQueueProvider& getMessageProvider() const = 0;
};

namespace Lua {
  std::unique_ptr<ILuaGameContext> createGameContext(LuaGameSystem& system, std::unique_ptr<Lua::State> state);
  ILuaGameContext& checkGameContext(lua_State* l);
}
