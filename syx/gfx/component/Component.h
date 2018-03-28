#pragma once

class MessageQueueProvider;

namespace Lua {
  class Node;
};

#define DEFINE_COMPONENT(compType, ...) namespace {\
    static Component::Registry::Registrar compType##_reg(Component::typeId<compType>(), [](Handle h) {\
      return std::make_unique<compType>(h);\
    });\
  }\
  compType::compType(Handle owner)\
    : Component(Component::typeId<compType>(), owner)

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

  virtual const Lua::Node* getLuaProps() const;

protected:
  Handle mOwner;
  size_t mType;
};