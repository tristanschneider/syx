#pragma once

class Event;
class EventBuffer;

class EventHandler {
public:
  using Callback = std::function<void(const Event&)>;

  template<class EventType, class Func>
  void registerEventHandler(Func func) {
    registerEventHandler(Event::typeId<EventType>(), [f = std::move(func)](const Event& e) {
      f(static_cast<const EventType&>(e));
    });
  }

  void registerEventHandler(size_t type, Callback h);
  void registerGlobalHandler(Callback h);
  void handleEvents(const EventBuffer& buffer);

private:
  TypeMap<Callback> mEventHandlers;
  Callback mGlobalHandler;
};