#pragma once
#include "component/Transform.h"

class Component;
class LuaComponent;
struct lua_State;

namespace Lua {
  class Cache;
}

class LuaGameObject {
public:
  LuaGameObject(Handle h);
  ~LuaGameObject();

  Handle getHandle() const;

  void addComponent(std::unique_ptr<Component> component);
  void removeComponent(size_t type);
  Component* getComponent(size_t type);
  Transform& getTransform();

  template<typename CompType>
  CompType* getComponent() {
    std::unique_ptr<Component>* result = mComponents.get<CompType>();
    return result ? static_cast<CompType*>(result->get()) : nullptr;
  }

  template<typename CompType>
  void removeComponent() {
    mComponents.set<CompType>(nullptr);
  }

  LuaComponent* addLuaComponent(size_t script);
  LuaComponent* getLuaComponent(size_t script);
  void removeLuaComponent(size_t script);
  std::unordered_map<size_t, LuaComponent>& getLuaComponents();

  static void openLib(lua_State* l);
  static int toString(lua_State* l);
  static int indexOverload(lua_State* l);
  //Component addComponent(string componentName)
  static int addComponent(lua_State* l);
  //void removeComponent(string componentName)
  static int removeComponent(lua_State* l);
  //bool isValid()
  static int isValid(lua_State* l);

  //static LuaGameObject newDefault()
  static int newDefault(lua_State* l);

  static int push(lua_State* l, LuaGameObject& obj);
  static int invalidate(lua_State* l, LuaGameObject& obj);

  static LuaGameObject& getObj(lua_State* l, int index);

private:
  static const std::string CLASS_NAME;
  static std::unique_ptr<Lua::Cache> sCache;

  Handle mHandle;
  Transform mTransform;
  TypeMap<std::unique_ptr<Component>, Component> mComponents;
  //Id of script in asset repo to the lua component holding it
  std::unordered_map<size_t, LuaComponent> mLuaComponents;
  //Name hash to component or lua component
  std::unordered_map<size_t, Component*> mHashToComponent;
};