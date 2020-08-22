#pragma once
#include "Handle.h"
#include "util/TypeId.h"

class AssetRepo;
class Camera;
class ComponentPublisher;
class DebugDrawer;
class EventBuffer;
struct IComponent;
class LuaGameObject;
class LuaGameObjectProvider;
class LuaGameSystem;
class MessageQueueProvider;
class System;

struct lua_State;
struct luaL_Reg;

namespace Lua {
  class Cache;
  class Node;
  using NodeDiff = uint64_t;
};

#define WRAP_BASE_FUNC(func) static int func(lua_State* l) { return Component::func(l, singleton().getTypeInfo().mTypeName); }
//Implement base functions with wrappers for the derived class
#define COMPONENT_LUA_INHERIT(type)\
  static const type& singleton() { static type s(0); return s; }\
  WRAP_BASE_FUNC(_indexOverload)\
  WRAP_BASE_FUNC(_newIndexOverload)\
  WRAP_BASE_FUNC(_getName)\
  WRAP_BASE_FUNC(_getType)\
  WRAP_BASE_FUNC(_getOwner)\
  WRAP_BASE_FUNC(_getProps)\
  WRAP_BASE_FUNC(_setProps)\
  WRAP_BASE_FUNC(_getProp)\
  WRAP_BASE_FUNC(_setProp)

#define COMPONENT_LUA_BASE_REGS \
  { "__index", _indexOverload },\
  { "__newindex", _newIndexOverload },\
  { "getName", _getName },\
  { "getType", _getType },\
  { "getOwner", _getOwner },\
  { "getProps", _getProps },\
  { "setProps", _setProps },\
  { "getProp", _getProp },\
  { "setProp", _setProp }

struct ComponentTypeInfo {
  ComponentTypeInfo(const std::string& typeName);

  //Name of the lua type
  std::string mTypeName;
  //Name of the property on game objects
  std::string mPropName;
  size_t mPropNameConstHash;
};

//TODO: use this everywhere instead of separate values
struct ComponentType {
  size_t id;
  size_t subId;

  size_t operator()() const;
  const bool operator==(const ComponentType& rhs) const;
  const bool operator!=(const ComponentType& rhs) const;
};

class Component {
public:
  DECLARE_TYPE_CATEGORY;

  struct EditorUpdateArgs {
    const LuaGameObjectProvider& objects;
    DebugDrawer& drawer;
    MessageQueueProvider& msg;
    const LuaGameObject& editorCamera;
  };

  Component(Handle type, Handle owner);
  virtual ~Component();

  template<typename T>
  static size_t typeId() {
    return ::typeId<T, Component>();
  }

  size_t getType() const {
    return mType;
  }
  size_t getSubType() const {
    return mSubType;
  }
  //TODO: replace the above two with this
  ComponentType getFullType() const {
    return { mType, mSubType };
  }
  Handle getOwner() const {
    return mOwner;
  }
  void setSubType(size_t subType) {
    mSubType = subType;
    _setSubType(subType);
  }

  void setSystem(System& system);
  System* getSystem() const;
  AssetRepo* getAssetRepo() const;

  //Push lua component onto stack
  static int push(lua_State* l, IComponent& component);
  void invalidate(lua_State* l) const;

  void addSync(EventBuffer& msg) const;
  void sync(EventBuffer& msg, Lua::NodeDiff diff = ~0) const;

  virtual std::unique_ptr<Component> clone() const = 0;
  //Base version sets this object based on its properties, derived types can do explicit assignment as an optimization
  virtual void set(const Component& component) = 0;

  virtual const Lua::Node* getLuaProps() const;
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;
  virtual void onPropsUpdated() {}

  virtual void onEditorUpdate(const LuaGameObject&, bool, EditorUpdateArgs&) const {}

  static void setPropsFromStack(lua_State* l, IComponent& component);
  static void setPropFromStack(lua_State* l, IComponent& component, const char* name);

  static void baseOpenLib(lua_State* l);
  static int _getName(lua_State* l, const std::string& type);
  static int _getType(lua_State* l, const std::string& type);
  static int _getOwner(lua_State* l, const std::string& type);
  static int _getProps(lua_State* l, const std::string& type);
  static int _setProps(lua_State* l, const std::string& type);
  static int _getProp(lua_State* l, const std::string& type);
  static int _setProp(lua_State* l, const std::string& type);
  static int _indexOverload(lua_State* l, const std::string& type);
  static int _newIndexOverload(lua_State* l, const std::string& type);

  static IComponent& _checkSelf(lua_State* l, const std::string& type, int arg = 1);

  static const std::string LUA_PROPS_KEY;

protected:
  static const Lua::Cache& getLuaCache();

  const Lua::Node* _getPropByName(const char* propName) const;

  virtual void _setSubType(size_t) {}

  Handle mOwner;
  size_t mType;
  size_t mSubType;
  size_t mCacheId;
  System* mSystem = nullptr;

private:
  static const std::string BASE_CLASS_NAME;
  static std::unique_ptr<Lua::Cache> sLuaCache;
};

//Convenience wrapper for constructor inheritance
template<class T>
class TypedComponent : public Component {
public:
  TypedComponent(Handle h)
    : Component(Component::typeId<T>(), h) {
  }
  TypedComponent(const TypedComponent& rhs)
    : Component(rhs.getType(), rhs.getOwner()) {
  }
  virtual ~TypedComponent() = default;
};