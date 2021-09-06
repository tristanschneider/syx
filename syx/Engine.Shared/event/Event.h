#pragma once

#include <event/EventBuffer.h>
#include "util/TypeId.h"

class System;

#define REGISTER_EVENT(type) namespace {\
  Event::Registry::Registrar type##_registrar(Event::typeId<type>(), [](const Event& e, uint8_t* buffer) {\
     new (buffer) type(static_cast<const type&>(e));\
  },\
  [](Event&& e, uint8_t* buffer) {\
    new (buffer) type(std::move(static_cast<type&&>(e)));\
  });\
}

//TODO: get rid of this in favor of TypedEvent
//Register event type and begin constructor, used like:
//DEFINE_EVENT(MyEventType, int constructorArgA, bool constructorArgB)
//  , mA(constructorArgA)
//  , mB(constructorArgB) {
//}
#define DEFINE_EVENT(eventType, ...) REGISTER_EVENT(eventType)\
  eventType::eventType(__VA_ARGS__)\
    : Event(Event::typeId<eventType>(), sizeof(eventType))

class Event {
public:
  class Registry {
  public:
    using CopyConstructor = std::function<void(const Event&, uint8_t*)>;
    using MoveConstructor = std::function<void(Event&&, uint8_t*)>;

    struct Registrar {
      Registrar(size_t type, CopyConstructor copy, MoveConstructor move) {
        registerEvent(type, copy, move);
      };
    };

    static void registerEvent(size_t type, CopyConstructor copy, MoveConstructor move);
    //Copy construct a new event from e at buffer
    static void copyConstruct(const Event& e, uint8_t& buffer);
    static void moveConstruct(Event&& e, uint8_t& buffer);

  private:
    static Registry& _get();
    Registry();

    TypeMap<std::pair<CopyConstructor, MoveConstructor>> mConstructors;
  };

  DECLARE_TYPE_CATEGORY

  Event(size_t type, size_t size);
  virtual ~Event();
  size_t getSize() const;
  size_t getType() const;

  template<typename T>
  static size_t typeId() {
    return ::typeId<T, Event>();
  }

private:
  size_t mType;
  size_t mSize;
};

template<class T>
class TypedEvent : public Event {
public:
  TypedEvent()
    : Event(Event::typeId<T>(), sizeof(T)) {
    //TODO: do this explicitly in AppRegistration
    static auto reg(Event::Registry::Registrar(Event::typeId<T>(), [](const Event& e, uint8_t* buffer) {
       new (buffer) T(static_cast<const T&>(e));
    },
    [](Event&& e, uint8_t* buffer) {
      new (buffer) T(std::move(static_cast<T&&>(e)));
    }));
  }
};

// Used to deliver a callback to a given system id. For a system to get the callback it must add the handler to its message handler.
class CallbackEvent : public TypedEvent<CallbackEvent> {
public:
  CallbackEvent(typeId_t<System> destinationId, std::function<void()>&& callback)
    : mDestId(destinationId)
    , mCallback(std::move(callback)) {
  }

  static std::function<void(const CallbackEvent&)> getHandler(typeId_t<System> systemId) {
    return [systemId](const CallbackEvent& e) { e.tryHandle(systemId); };
  }

  void tryHandle(typeId_t<System> systemId) const {
    if(mDestId == systemId) {
      mCallback();
    }
  }

private:
  typeId_t<System> mDestId;
  std::function<void()> mCallback;
};

//Used to request a response from some system and have the resulting handler be processed on the requester's message loop.
template<class Req, class Res>
class RequestEvent : public TypedEvent<Req> {
public:
  using ResponseHandler = std::function<void(const Res&)>;

  Req& then(typeId_t<System> requesterType, ResponseHandler&& responseHandler) {
    mRequesterType = requesterType;
    mHandler = std::move(responseHandler);
    return static_cast<Req&>(*this);
  }

  void respond(EventBuffer& msg, Res res) const {
    //Make sure there is a handler listening for this response. If there isn't, either no one cares or this event has already been responded to
    if(mHandler) {
      //Copy the callback and the result and deliver it to the requester to be executed there
      ResponseHandler handlerCopy = std::move(mHandler);
      mHandler = nullptr;
      //The requester must have a handler for CallbackEvent or he won't get this response
      msg.push(CallbackEvent(mRequesterType, [res = std::move(res), handlerCopy = std::move(handlerCopy)]() {
        handlerCopy(res);
      }));
    }
  }

private:
  typeId_t<System> mRequesterType;
  mutable ResponseHandler mHandler;
};
