#include "Precompile.h"
#include "system/MessagingSystem.h"

void MessagingSystem::init() {
  mFrame = 0;
}

void MessagingSystem::update(float dt) {
  ++mFrame;
}

void MessagingSystem::addTransformListener(TransformListener& listener) {
  mTransformListeners.push_back(&listener);
}

void MessagingSystem::removeTransformListener(TransformListener& listener) {
  auto it = std::find(mTransformListeners.begin(), mTransformListeners.end(), &listener);
  if(it != mTransformListeners.end())
    mTransformListeners.erase(it);
}

void MessagingSystem::fireTransformEvent(TransformEvent& e) {
  e.mFrame = mFrame;
  for(TransformListener* l : mTransformListeners)
    l->mEvents.push_back(e);
}

void MessagingSystem::fireTransformEvents(std::vector<TransformEvent>& e) {
  for(TransformEvent& c : e)
    c.mFrame = mFrame;
  size_t size = sizeof(TransformEvent)*e.size();
  for(TransformListener* l : mTransformListeners) {
    std::vector<TransformEvent>& buff = l->mEvents;
    size_t oldSize = buff.size();
    buff.resize(oldSize + e.size());
    std::memcpy(&buff[oldSize], e.data(), size);
  }
}

TransformEvent::TransformEvent(Handle handle, Syx::Mat4 transform)
  : mFrame(0)
  , mHandle(handle)
  , mTransform(transform) {
}
