#pragma once
#include "util/FunctionTraits.h"

class Event;
class EventBuffer;

struct EventListener {
  virtual ~EventListener() = default;
  virtual void onEvent(const Event&) {}
};

class EventHandler {
public:
  using Callback = std::function<void(const Event&)>;

  //Register an event handler with a member function that looks something like void DerivedListener::onEvent(const EventT& event) {}
  template<class EventT, class DerivedListener>
  void registerEventListener(std::weak_ptr<DerivedListener> listener, void(DerivedListener::*callback)(const EventT&)) {
    static_assert(std::is_base_of_v<EventListener, DerivedListener>, "registerEventListener should only be used with event listeners");
    static_assert(std::is_base_of_v<Event, EventT>, "Callback should take an event type");

    CallbackHandler c;
    c.mListener = listener;
    c.mInvoker = [listener, callback](const Event& e) {
      if(auto self = listener.lock()) {
        (*self.*callback)(static_cast<const EventT&>(e));
      }
    };
    _registerEventListener(Event::typeId<EventT>(), std::move(c));
  }

  //Convenience wrapper to allow calling with strong references
  template<class EventT, class DerivedListener>
  void registerEventListener(std::shared_ptr<DerivedListener> listener, void(DerivedListener::*callback)(const EventT&)) {
    registerEventListener(std::weak_ptr<DerivedListener>(std::move(listener)), callback);
  }

  //Register an event handler that looks something like [](const MyEvent& event) {}
  //The caller must retain the listener for as long as they want their callback to be registered
  template<class Func>
  [[nodiscard]] std::shared_ptr<EventListener> registerEventListener(Func func) {
    using EventType = typename FunctionTraits<Func>::template argument<0>::type;
    using EventListenerT = CustomEventListener<Func, std::decay_t<EventType>>;

    auto result = std::make_shared<EventListenerT>(std::move(func));
    registerEventListener(Event::typeId<std::decay_t<EventType>>(), std::weak_ptr<EventListenerT>(result));
    return result;
  }

  void registerEventListener(size_t type, std::weak_ptr<EventListener> listener);
  void registerGlobalListener(std::weak_ptr<EventListener> listener);
  void handleEvents(const EventBuffer& buffer);

private:
  template<class FuncT, class EventType>
  struct CustomEventListener : public EventListener {
    CustomEventListener(FuncT f)
      : mFunc(std::move(f)) {
    }

    //This is only registered on one type so will be invoked on one type.
    void onEvent(const Event& e) {
      mFunc(static_cast<const EventType&>(e));
    }

    FuncT mFunc;
  };

  struct CallbackHandler {
    Callback mInvoker;
    std::weak_ptr<EventListener> mListener;
  };
  struct HandlerSlot {
    void invoke(const Event& e);

    std::vector<CallbackHandler> mHandlers;
  };

  static CallbackHandler _wrapListener(std::weak_ptr<EventListener> listener);
  void _registerEventListener(size_t type, CallbackHandler handler);

  TypeMap<HandlerSlot> mEventHandlers;
  HandlerSlot mGlobalHandler;
};