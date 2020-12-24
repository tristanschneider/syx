#pragma once
#include "system/System.h"

//A system to allow tests to get responses to RequestEvent types via CallbackEvent
class TestListenerSystem : public System {
public:
  enum class HandlerLifetime {
    //Event handler is removed after being invoked once
    SingleUse,
    //Event handler is never removed
    Persistent,
  };

  enum class HandlerResponse {
    //Continue on to other potentially registered event handlers
    Continue,
    //Stop processing, preventing any other registered event handlers from triggering
    Stop,
  };

  using HandlerCallback = std::function<void(const Event&, MessageQueueProvider&)>;

  TestListenerSystem(const SystemArgs& args);
  ~TestListenerSystem();
  void init() override;
  void update(float, IWorkerPool&, std::shared_ptr<Task>) override;

  //Get the event buffer of the system, meaning events received last updated
  const EventBuffer& getEventBuffer() const;
  //Was an event of this type recieved last update
  bool hasEventOfType(size_t type) const;
  const Event* tryGetEventOfType(size_t type) const;

  //Register a handler for an event of the given type. Handlers are invoked in order of registration if there are multiple for a single type
  TestListenerSystem& registerEventHandler(size_t eventType, HandlerLifetime lifetime, HandlerResponse response, HandlerCallback callback);
  TestListenerSystem& clearEventHandlers();

private:
  struct TestEventHandler {
    HandlerLifetime mLifetime = HandlerLifetime::SingleUse;
    HandlerResponse mResponse = HandlerResponse::Continue;
    HandlerCallback mCallback;
    size_t mEventType = 0;
  };

  //The event buffer is cleared at the end of the frame. For tests to be able to look at what was sent it is copied into this
  std::unique_ptr<EventBuffer> mEventBufferCopy;
  std::vector<TestEventHandler> mHandlers;
};
