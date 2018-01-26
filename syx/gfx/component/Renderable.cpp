#include "Precompile.h"
#include "Renderable.h"
#include "system/MessagingSystem.h"

DEFINE_EVENT(RenderableUpdateEvent, const RenderableData& data, Handle obj)
  , mObj(obj)
  , mData(data) {
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
  mMessaging->fireEvent(RenderableUpdateEvent(mData, mOwner));
}
