#pragma once
#include "System.h"

struct TransformEvent;
struct TransformListener;
struct EventListener;
class Event;

class MessagingSystem : public System {
public:
  RegisterSystemH(MessagingSystem);
  using System::System;

  void addTransformListener(TransformListener& listener);
  void removeTransformListener(TransformListener& listener);
  void fireTransformEvent(TransformEvent& e);
  void fireTransformEvents(std::vector<TransformEvent>& e, TransformListener* except = nullptr);

  void addEventListener(EventListener& listener);
  void removeEventListener(EventListener& listener);
  void fireEvent(std::unique_ptr<Event> e);

  std::vector<TransformListener*> mTransformListeners;
  std::vector<EventListener*> mEventListeners;
};
