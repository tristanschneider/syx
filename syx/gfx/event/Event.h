#pragma once

#define REGISTER_EVENT(type) namespace {\
  Event::Registry::Registrar type##_registrar(Event::typeId<type, Event>(), [](const Event& e, uint8_t* buffer) {\
     new (buffer) type(static_cast<const type&>(e));\
  });\
}

//Register event type and begin constructor, used like:
//DEFINE_EVENT(MyEventType, int constructorArgA, bool constructorArgB)
//  , mA(constructorArgA)
//  , mB(constructorArgB) {
//}
//  REGISTER_EVENT(type)
#define DEFINE_EVENT(eventType, ...) REGISTER_EVENT(eventType)\
  eventType::eventType(__VA_ARGS__)\
    : Event(typeId<eventType>(), sizeof(eventType))

namespace Test {
  void TestEvents();

  class Event;

  class EventListener {
  public:
    using EventHandler = std::function<void(const Event&)>;

    EventListener();
    //No reason to copy this, so any copies would likely be accidental
    EventListener(const EventListener&) = delete;
    EventListener& operator=(const EventListener&) = delete;

    void push(const Event& e);
    void registerEventHandler(size_t type, EventHandler h);
    void registerGlobalHandler(EventHandler h);
    void appendTo(EventListener& listener) const;
    void handleEvents();

  private:
    std::vector<uint8_t> mBuffer;
    TypeMap<EventHandler> mEventHandlers;
    EventHandler mGlobalHandler;
  };

  class Event {
  public:
    class Registry {
    public:
      using Constructor = std::function<void(const Event&, uint8_t*)>;

      struct Registrar {
        Registrar(size_t type, Constructor c) {
          registerEvent(type, c);
        };
      };

      static void registerEvent(size_t type, Constructor c);
      //Copy construct a new event from e at buffer
      static void construct(const Event& e, uint8_t& buffer);

    private:
      static Registry& _get();
      Registry();

      TypeMap<Constructor> mConstructors;
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

  class TestEvent : public Event {
  public:
    TestEvent(int a);

    int mA;
  };

  class TestEvent2 : public Event {
  public:
    TestEvent2(int a, bool b, char c);
    int mA;
    bool mB;
    char mC;
  };
}

enum class EventFlag : uint8_t {
  Invalid = 0,
  Graphics = 1 << 0,
  Physics = 1 << 1,
  Component = 1 << 2
};
MAKE_BITWISE_ENUM(EventFlag);

enum class EventType : uint8_t {
  PhysicsCompUpdate,
  RenderableUpdate,
  AddComponent,
  RemoveComponent
};

class Event {
public:
  Event(EventFlag flags)
    : mFlags(flags) {
  }

  //Handle describes the specific event type
  virtual Handle getHandle() const = 0;
  virtual std::unique_ptr<Event> clone() const = 0;

  //Flags describe event category
  EventFlag getFlags() const {
    return mFlags;
  }

protected:
  EventFlag mFlags;
};

struct EventListener {
  EventListener(EventFlag listenFlags)
    : mListenFlags(listenFlags) {
  }

  void updateLocal() {
    mLocalEvents.clear();
    mMutex.lock();
    mLocalEvents.swap(mEvents);
    mMutex.unlock();
  }

  std::vector<std::unique_ptr<Event>> mEvents;
  //Local buffer used to spend as little time as possible locking event queues
  std::vector<std::unique_ptr<Event>> mLocalEvents;
  EventFlag mListenFlags;
  std::mutex mMutex;
};