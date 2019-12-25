#pragma once

class AssetRepo;
class Camera;
class DebugDrawer;
class EventBuffer;
class MessageQueueProvider;
class LuaGameObject;
class LuaGameObjectProvider;
class LuaGameSystem;
class System;

struct lua_State;
struct luaL_Reg;

namespace Lua {
  class Cache;
  class Node;
  using NodeDiff = uint64_t;
};

#define DEFINE_COMPONENT(compType, ...) namespace {\
    static Component::Registry::Registrar compType##_reg(Component::typeId<compType>(), [](Handle h) {\
      return std::make_unique<compType>(h);\
    });\
  }\
  compType::compType(Handle owner)\
    : Component(Component::typeId<compType>(), owner)

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
};

class Component {
public:
  DECLARE_TYPE_CATEGORY;

  class Registry {
  public:
    using Constructor = std::function<std::unique_ptr<Component>(Handle)>;

    struct Registrar {
      Registrar(size_t type, Constructor ctor);
    };

    static void registerComponent(size_t type, Constructor ctor);
    static std::unique_ptr<Component> construct(size_t type, Handle owner);
    static const TypeMap<Constructor, Component>& getConstructors();
  private:
    Registry();
    static Registry& _get();

    TypeMap<Constructor, Component> mCtors;
  };

  struct EditorUpdateArgs {
    const LuaGameObjectProvider& objects;
    DebugDrawer& drawer;
    MessageQueueProvider& msg;
    const LuaGameObject& editorCamera;
  };

  Component(Handle type, Handle owner);
  ~Component();

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
  int push(lua_State* l);
  void invalidate(lua_State* l) const;

  void addSync(EventBuffer& msg) const;
  void sync(EventBuffer& msg, Lua::NodeDiff diff = ~0) const;

  virtual std::unique_ptr<Component> clone() const = 0;
  virtual void set(const Component& component) = 0;

  virtual const Lua::Node* getLuaProps() const;
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;
  virtual void onPropsUpdated() {}

  virtual void onEditorUpdate(const LuaGameObject&, bool, EditorUpdateArgs&) const {}

  void setPropsFromStack(lua_State* l, LuaGameSystem& game) const;
  void setPropFromStack(lua_State* l, const char* name, LuaGameSystem& game) const;

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