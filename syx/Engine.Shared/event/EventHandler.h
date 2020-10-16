#pragma once
#include "util/FunctionTraits.h"

class Event;
class EventBuffer;

class EventHandler {
public:
  using Callback = std::function<void(const Event&)>;

  //Register an event handler that looks something like [](const MyEvent& event) {}
  template<class Func>
  void registerEventHandler(Func func) {
    using EventType = typename FunctionTraits<Func>::template argument<0>::type;
    //Func might be mutable, so make this mutable too so such a case would compile
    registerEventHandler(Event::typeId<std::decay_t<EventType>>(), [f = std::move(func)](const Event& e) mutable {
      //This cast forces event handlers to take the event by const reference
      f(static_cast<EventType>(e));
    });
  }

  void registerEventHandler(size_t type, Callback h);
  void registerGlobalHandler(Callback h);
  void handleEvents(const EventBuffer& buffer);

private:
  TypeMap<Callback> mEventHandlers;
  Callback mGlobalHandler;
};