#pragma once
#include "System.h"

struct TransformEvent;
struct TransformListener;
struct EventListener;
class Event;

class MessagingSystem : public System {
public:
  typedef uint8_t FrameId;

  SystemId getId() const {
    return SystemId::Messaging;
  }

  void init() override;
  void update(float dt) override;

  void addTransformListener(TransformListener& listener);
  void removeTransformListener(TransformListener& listener);
  void fireTransformEvent(TransformEvent& e);
  void fireTransformEvents(std::vector<TransformEvent>& e, TransformListener* except = nullptr);

  void addEventListener(EventListener& listener);
  void removeEventListener(EventListener& listener);
  void fireEvent(std::unique_ptr<Event> e);

  std::vector<TransformListener*> mTransformListeners;
  std::vector<EventListener*> mEventListeners;
  FrameId mFrame;
};

struct TransformListener {
  std::vector<TransformEvent> mEvents;
};

struct TransformEvent {
  TransformEvent() {}
  TransformEvent(Handle handle, Syx::Mat4 transform);

  MessagingSystem::FrameId mFrame;
  Handle mHandle;
  Syx::Mat4 mTransform;
};
