#pragma once

#define REGISTER_EVENT(type) namespace {\
  Event::Registry::Registrar type##_registrar(Event::typeId<type>(), [](const Event& e, uint8_t* buffer) {\
     new (buffer) type(static_cast<const type&>(e));\
  },\
  [](Event&& e, uint8_t* buffer) {\
    new (buffer) type(std::move(static_cast<type&&>(e)));\
  });\
}

//Register event type and begin constructor, used like:
//DEFINE_EVENT(MyEventType, int constructorArgA, bool constructorArgB)
//  , mA(constructorArgA)
//  , mB(constructorArgB) {
//}
#define DEFINE_EVENT(eventType, ...) REGISTER_EVENT(eventType)\
  eventType::eventType(__VA_ARGS__)\
    : Event(Event::typeId<eventType>(), sizeof(eventType))

class Event;

class EventListener {
public:
  using EventHandler = std::function<void(const Event&)>;

  EventListener(size_t baseCapacity = 256);
  //No reason to copy this, so any copies would likely be accidental
  EventListener(const EventListener&) = delete;
  EventListener& operator=(const EventListener&) = delete;

  void push(Event&& e);
  void push(const Event& e);
  void registerEventHandler(size_t type, EventHandler h);
  void registerGlobalHandler(EventHandler h);
  void appendTo(EventListener& listener) const;
  void handleEvents();
  void clear();

private:
  void _growBuffer(size_t bytes);

  uint8_t* mBuffer;
  size_t mBufferSize;
  size_t mBufferCapacity;
  TypeMap<EventHandler> mEventHandlers;
  EventHandler mGlobalHandler;
};

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