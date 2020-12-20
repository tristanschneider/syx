#pragma once
#include "system/System.h"

//A system to allow tests to get responses to RequestEvent types via CallbackEvent
class TestListenerSystem : public System {
public:
  TestListenerSystem(const SystemArgs& args);
  ~TestListenerSystem();
  void init() override;
  void update(float, IWorkerPool&, std::shared_ptr<Task>) override;

  //Get the event buffer of the system, meaning events received last updated
  const EventBuffer& getEventBuffer() const;
  //Was an event of this type recieved last update
  bool hasEventOfType(size_t type) const;
  const Event* tryGetEventOfType(size_t type) const;

private:
  //The event buffer is cleared at the end of the frame. For tests to be able to look at what was sent it is copied into this
  std::unique_ptr<EventBuffer> mEventBufferCopy;
};
