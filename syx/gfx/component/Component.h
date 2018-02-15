#pragma once

class MessageQueueProvider;

#define DEFINE_COMPONENT(compType, ...) compType::compType(Handle owner, MessageQueueProvider* messaging)\
  : Component(Component::typeId<compType>(), owner, messaging)

class Component {
public:
  DECLARE_TYPE_CATEGORY;
  Component(Handle type, Handle owner, MessageQueueProvider* messaging);
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

protected:
  Handle mOwner;
  MessageQueueProvider* mMessaging;
  size_t mType;
};