#include "Precompile.h"
#include "Renderable.h"
#include "system/MessagingSystem.h"

RenderableUpdateEvent::RenderableUpdateEvent(const RenderableData& data, Handle obj)
  : Event(EventFlag::Graphics)
  , mObj(obj)
  , mData(data) {
}

Handle RenderableUpdateEvent::getHandle() const {
  return static_cast<Handle>(EventType::RenderableUpdate);
}

std::unique_ptr<Event> RenderableUpdateEvent::clone() const {
  return std::make_unique<RenderableUpdateEvent>(mData, mObj);
}

Renderable::Renderable(Handle owner, MessagingSystem& messaging)
  : Component(static_cast<Handle>(ComponentType::Graphics), owner, &messaging) {
  mData.mModel = mData.mDiffTex = InvalidHandle;
}

const RenderableData& Renderable::get() const {
  return mData;
}

void Renderable::set(const RenderableData& data) {
  mData = data;
  _fireUpdate();
}

void Renderable::_fireUpdate() {
  mMessaging->fireEvent(std::make_unique<RenderableUpdateEvent>(mData, mOwner));
}
