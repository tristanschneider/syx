#pragma once

class MessageQueueProvider;

struct lua_State;
struct luaL_Reg;

namespace Lua {
  class Node;
  class Cache;
};

#define DEFINE_COMPONENT(compType, ...) namespace {\
    static Component::Registry::Registrar compType##_reg(Component::typeId<compType>(), [](Handle h) {\
      return std::make_unique<compType>(h);\
    });\
  }\
  compType::compType(Handle owner)\
    : Component(Component::typeId<compType>(), owner)

#define WRAP_BASE_FUNC(func, name) static int func(lua_State* l) { return func(l, name); }
//Implement base functions with wrappers for the derived class
#define COMPONENT_LUA_INHERIT(name)\
  WRAP_BASE_FUNC(getName, name)\
  WRAP_BASE_FUNC(getType, name)\
  WRAP_BASE_FUNC(getOwner, name)\
  WRAP_BASE_FUNC(getProps, name)

#define COMPONENT_LUA_BASE_REGS \
  { "getName", &getName },\
  { "getType", &getType },\
  { "getOwner", &getOwner },\
  { "getProps", &getProps },

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
  private:
    Registry();
    static Registry& _get();

    TypeMap<Constructor, Component> mCtors;
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
  Handle getOwner() {
    return mOwner;
  }

  //Push lua component onto stack
  int push(lua_State* l);
  void invalidate(lua_State* l) const;

  virtual const Lua::Node* getLuaProps() const;
  virtual void openLib(lua_State* l) const;
  //Name is the name the component appears as on the gameobject, while typename is the name of the lua type for the component
  virtual const std::string& getName() const;
  virtual const std::string& getTypeName() const;
  virtual size_t getNameConstHash() const;

  static void baseOpenLib(lua_State* l);
  static int getName(lua_State* l, const std::string& type);
  static int getType(lua_State* l, const std::string& type);
  static int getOwner(lua_State* l, const std::string& type);
  static int getProps(lua_State* l, const std::string& type);

  static const std::string LUA_PROPS_KEY;

protected:
  const Lua::Cache& getLuaCache() const;

  Handle mOwner;
  size_t mType;
  size_t mCacheId;

private:
  static const std::string BASE_CLASS_NAME;
  static std::unique_ptr<Lua::Cache> sLuaCache;
};