#include "Precompile.h"
#include "system/MessagingSystem.h"
#include "event/Event.h"
#include "event/TransformEvent.h"

RegisterSystemCPP(MessagingSystem);

void MessagingSystem::addTransformListener(TransformListener& listener) {
  mTransformListeners.push_back(&listener);
}

void MessagingSystem::removeTransformListener(TransformListener& listener) {
  auto it = std::find(mTransformListeners.begin(), mTransformListeners.end(), &listener);
  if(it != mTransformListeners.end())
    mTransformListeners.erase(it);
}

void MessagingSystem::fireTransformEvent(TransformEvent& e) {
  for(TransformListener* l : mTransformListeners) {
    l->mMutex.lock();
    l->mEvents.push_back(e);
    l->mMutex.unlock();
  }
}

void MessagingSystem::fireTransformEvents(std::vector<TransformEvent>& e, TransformListener* except) {
  if(e.empty())
    return;

  size_t size = sizeof(TransformEvent)*e.size();
  for(TransformListener* l : mTransformListeners) {
    if(l == except)
      continue;

    l->mMutex.lock();
    std::vector<TransformEvent>& buff = l->mEvents;
    size_t oldSize = buff.size();
    buff.resize(oldSize + e.size());
    std::memcpy(&buff[oldSize], e.data(), size);
    l->mMutex.unlock();
  }
}

void MessagingSystem::addEventListener(EventListener& listener) {
  mEventListeners.push_back(&listener);
}

void MessagingSystem::removeEventListener(EventListener& listener) {
  auto it = std::find(mEventListeners.begin(), mEventListeners.end(), &listener);
  if(it != mEventListeners.end())
    mEventListeners.erase(it);
}

void MessagingSystem::fireEvent(std::unique_ptr<Event> e) {
  EventFlag flags = e->getFlags();
  for(size_t i = 0; i + 1 < mEventListeners.size(); ++i) {
    EventListener* listener = mEventListeners[i];
    if(static_cast<uint8_t>(listener->mListenFlags & flags)) {
      std::unique_ptr<Event> temp = e->clone();
      EventListener* listener = mEventListeners[i];

      listener->mMutex.lock();
      listener->mEvents.push_back(std::move(e));
      listener->mMutex.unlock();

      e = std::move(temp);
    }
  }
  if(mEventListeners.size()) {
    EventListener* listener = mEventListeners.back();

    listener->mMutex.lock();
    mEventListeners.back()->mEvents.push_back(std::move(e));
    listener->mMutex.unlock();
  }
}
