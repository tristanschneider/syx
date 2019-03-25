#include "Precompile.h"
#include "editor/event/EditorEvents.h"

SetSelectionEvent::SetSelectionEvent(std::vector<Handle>&& objects)
  : mObjects(std::move(objects)) {
}

ScreenPickRequest::ScreenPickRequest(size_t requestId, Handle space, const Syx::Vec2& min, const Syx::Vec2& max)
  : mRequestId(requestId)
  , mSpace(space)
  , mMin(min)
  , mMax(max) {
}

ScreenPickResponse::ScreenPickResponse(size_t requestId, Handle space, std::vector<Handle>&& objects)
  : mRequestId(requestId)
  , mSpace(space)
  , mObjects(std::move(objects)) {
}

PreviewAssetEvent::PreviewAssetEvent(AssetInfo asset)
  : mAsset(std::move(asset)) {
}

SetPlayStateEvent::SetPlayStateEvent(PlayState state)
  : mState(state) {
}