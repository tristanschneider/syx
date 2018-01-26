#include "Precompile.h"
#include "system/MessagingSystem.h"
#include "event/Event.h"
#include "event/TransformEvent.h"

RegisterSystemCPP(MessagingSystem);

void MessagingSystem::init() {
  mListener = std::make_unique<EventListener>();
}

void MessagingSystem::fireEvent(const Event& e) {
  mListener->push(e);
}

void MessagingSystem::fireEvent(Event&& e) {
  mListener->push(std::move(e));
}

EventListener& MessagingSystem::getListener() {
  return *mListener;
}