#pragma once
#include "component/SpaceComponent.h"
#include "component/Transform.h"

class Component;
class LuaComponent;
struct lua_State;

namespace Lua {
  class Cache;
  class Node;
}

struct LuaGameObjectDescription {
  //Needs overload for lua node
  LuaGameObjectDescription() = default;
  LuaGameObjectDescription(LuaGameObjectDescription&&) = default;
  bool operator==(const LuaGameObjectDescription&) const { return false; }
  LuaGameObjectDescription& operator=(const LuaGameObjectDescription&) = delete;
  LuaGameObjectDescription& operator=(LuaGameObjectDescription&&) = default;
  const Lua::Node& getMetadata() const;

  size_t mHandle;
  std::vector<std::unique_ptr<Component>> mComponents;

private:
  std::unique_ptr<Lua::Node> _buildMetadata() const;
};

struct LuaSceneDescription {
  //Metadata writes the scene to this global
  static const char* ROOT_KEY;
  static const char* FILE_EXTENSION;

  const Lua::Node& getMetadata() const;

  std::string mName;
  std::vector<LuaGameObjectDescription> mObjects;

private:
  std::unique_ptr<Lua::Node> _buildMetadata() const;
};

class LuaGameObject {
public:
  LuaGameObject(Handle h);
  LuaGameObject(const LuaGameObject&) = delete;
  ~LuaGameObject();

  Handle getHandle() const;

  void addComponent(std::unique_ptr<Component> component);
  void removeComponent(size_t type);
  Component* getComponent(size_t type);
  const Component* getComponent(size_t type) const;
  Transform& getTransform();
  const Transform& getTransform() const;
  Handle getSpace() const;

  template<typename CompType>
  CompType* getComponent() {
    std::unique_ptr<Component>* result = mComponents.get<CompType>();
    return result ? static_cast<CompType*>(result->get()) : nullptr;
  }
  template<typename CompType>
  const CompType* getComponent() const {
    std::unique_ptr<Component> const* result = mComponents.get<CompType>();
    return result ? static_cast<const CompType*>(result->get()) : nullptr;
  }

  template<typename CompType>
  void removeComponent() {
    mComponents.set<CompType>(nullptr);
  }

  LuaComponent* addLuaComponent(size_t script);
  LuaComponent* getLuaComponent(size_t script);
  void removeLuaComponent(size_t script);
  std::unordered_map<size_t, LuaComponent>& getLuaComponents();
  const std::unordered_map<size_t, LuaComponent>& getLuaComponents() const;
  const TypeMap<std::unique_ptr<Component>, Component>& getComponents() const;
  void forEachComponent(std::function<void(const Component&)> callback) const;

  static void openLib(lua_State* l);
  static int toString(lua_State* l);
  static int indexOverload(lua_State* l);
  static int newIndexOverload(lua_State* l);
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

  void _addBuiltInComponents();
  void _addComponentLookup(Component& comp);
  void _removeComponentLookup(const Component& comp);

  Handle mHandle;
  Transform mTransform;
  SpaceComponent mSpace;
  TypeMap<std::unique_ptr<Component>, Component> mComponents;
  //Id of script in asset repo to the lua component holding it
  std::unordered_map<size_t, LuaComponent> mLuaComponents;
  //Name hash to component or lua component
  std::unordered_map<size_t, Component*> mHashToComponent;
};