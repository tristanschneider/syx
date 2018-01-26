#pragma once
#include "System.h"

class TransformEvent;
class EventListener;
class Event;

class MessagingSystem : public System {
public:
  RegisterSystemH(MessagingSystem);
  using System::System;

  void init() override;

  void fireEvent(Event&& e);
  void fireEvent(const Event& e);

  EventListener& getListener();

private:
  std::unique_ptr<EventListener> mListener;
};
