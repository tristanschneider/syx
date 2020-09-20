#pragma once
#include "component/LuaComponent.h"
#include "component/NameComponent.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"

class Component;
class EventBuffer;
struct IGameObject;
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
};

struct LuaSceneDescription {
  //Metadata writes the scene to this global
  static const char* ROOT_KEY;
  static const char* FILE_EXTENSION;

  const Lua::Node& getMetadata() const;

  std::string mName;
  std::vector<LuaGameObjectDescription> mObjects;
  std::vector<std::string> mAssets;
};

class LuaGameObject {
public:
  static const std::string CLASS_NAME;

  LuaGameObject(Handle h);
  LuaGameObject(const LuaGameObject&) = delete;
  ~LuaGameObject();

  Handle getHandle() const;

  void addComponent(std::unique_ptr<Component> component);
  void removeComponent(size_t type);
  void removeComponent(const ComponentType& type);
  //TODO: get rid of these sub type overloads
  Component* getComponent(size_t type, size_t subType = 0);
  const Component* getComponent(size_t type, size_t subType = 0) const;
  Component* getComponent(const ComponentType& type);
  const Component* getComponent(const ComponentType& type) const;
  const Component* getComponent(const char* name) const;
  Transform& getTransform();
  const Transform& getTransform() const;
  const NameComponent& getName() const;
  Handle getSpace() const;

  template<typename CompType>
  CompType* getComponent() {
    return static_cast<CompType*>(getComponent(Component::typeId<CompType>()));
  }
  template<typename CompType>
  const CompType* getComponent() const {
    return static_cast<const CompType*>(getComponent(Component::typeId<CompType>()));
  }

  template<typename CompType>
  void removeComponent() {
    mComponents.set<CompType>(nullptr);
  }

  void remove(EventBuffer& msg) const;

  LuaComponent* addLuaComponent(std::unique_ptr<LuaComponent> component);
  LuaComponent* addLuaComponent(size_t script);
  LuaComponent* getLuaComponent(size_t script);
  const LuaComponent* getLuaComponent(size_t script) const;
  void removeLuaComponent(size_t script);

  template<class Func>
  void forEachLuaComponent(const Func& func) {
    for(std::unique_ptr<LuaComponent>& c : mLuaComponents)
      func(*c);
  }

  const TypeMap<std::unique_ptr<Component>, Component>& getComponents() const;
  template<class Func>
  void forEachComponent(const Func& callback) const {
    for(const auto& c : mComponents)
      callback(*c);
    for(const auto& c : mLuaComponents)
      callback(*c);
    _forEachBuiltInComponent(callback);
  }

  template<class Func>
  void forEachComponent(const Func& callback) {
    for(const auto& c : mComponents)
      callback(*c);
    for(const auto& c : mLuaComponents)
      callback(*c);
    _forEachBuiltInComponent(callback);
  }

  size_t componentCount() const;

  std::unique_ptr<LuaGameObject> clone() const;

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

  static int push(lua_State* l, IGameObject& obj);

  static IGameObject& getObj(lua_State* l, int index);

  bool _isBuiltInComponent(const Component& comp) const;

private:
  void _addBuiltInComponents();

  template<class Func>
  void _forEachBuiltInComponent(const Func& func) {
    const_cast<const LuaGameObject*>(this)->_forEachBuiltInComponent([func](const Component& c) {
      func(const_cast<Component&>(c));
    });
  }

  template<class Func>
  void _forEachBuiltInComponent(const Func& func) const {
    func(mTransform);
    func(mSpace);
    func(mName);
  }

  void _addComponentLookup(Component& comp);
  void _removeComponentLookup(const Component& comp);

  Handle mHandle;
  Transform mTransform;
  SpaceComponent mSpace;
  NameComponent mName;
  TypeMap<std::unique_ptr<Component>, Component> mComponents;
  std::vector<std::unique_ptr<LuaComponent>> mLuaComponents;
  //Name hash to component or lua component
  std::unordered_map<size_t, Component*> mHashToComponent;
};