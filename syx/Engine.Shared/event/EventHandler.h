#pragma once
#include "util/FunctionTraits.h"
#include "threading/NoLock.h"

class Event;
class EventBuffer;

struct EventListener {
  virtual ~EventListener() = default;
  virtual void onEvent(const Event&) {}
};

class EventHandler;

//Convenience class for wrappers that listen to events to cache latest state
template<class StoredT>
struct EventStore : public EventListener {
  virtual ~EventStore() = default;
  virtual void init(EventHandler& handler) = 0;
  //Get the latest version of the value. Return by value to allow thread-safe implementations if desired
  virtual StoredT get() const = 0;
  //View for still allowing optional thread safety while preventing copies of the viewed object
  virtual void view(const std::function<void(const StoredT&)>& viewer) const = 0;
  //Clears the dirty flag and returns the previous value. Can be used to know if the value changed
  virtual bool tryClearDirty() = 0;
};

//Another convenience class to implement value get/set with optional thread safety
template<class DerivedT, class StoredT, class LockT = NoLock>
struct EventStoreImpl : public EventStore<StoredT>, public std::enable_shared_from_this<EventStoreImpl<DerivedT, StoredT, LockT>> {
  using LockGuard = std::lock_guard<LockT>;

  StoredT get() const override {
    LockGuard lock(mMutex);
    return mValue;
  }

  virtual void view(const std::function<void(const StoredT&)>& viewer) const override {
    LockGuard lock(mMutex);
    viewer(mValue);
  }

  bool tryClearDirty() override {
    LockGuard lock(mMutex);
    const bool result = mDirty;
    mDirty = false;
    return result;
  }

protected:
  void _set(StoredT value) {
    LockGuard lock(mMutex);
    mValue = std::move(value);
    mDirty = true;
  }

  //Set the value through a lambda that takes the value by reference
  template<class Setter>
  void _set(const Setter& setter) {
    LockGuard lock(mMutex);
    setter(mValue);
    mDirty = true;
  }

  std::shared_ptr<DerivedT> _sharedFromThis() {
    return std::static_pointer_cast<DerivedT>(shared_from_this());
  }

private:
  StoredT mValue;
  mutable LockT mMutex;
  bool mDirty = false;
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