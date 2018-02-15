#include "Precompile.h"
#include "Renderable.h"

#include "event/EventBuffer.h"
#include "provider/MessageQueueProvider.h"

DEFINE_EVENT(RenderableUpdateEvent, const RenderableData& data, Handle obj)
  , mObj(obj)
  , mData(data) {
}

DEFINE_COMPONENT(Renderable) {
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
  mMessaging->getMessageQueue().get().push(RenderableUpdateEvent(mData, mOwner));
}
