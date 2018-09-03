#include "Precompile.h"
#include "editor/event/EditorEvents.h"

DEFINE_EVENT(SetSelectionEvent, std::vector<Handle>&& objects)
  , mObjects(std::move(objects)) {
}

DEFINE_EVENT(ScreenPickRequest, size_t requestId, Handle space, const Syx::Vec2& min, const Syx::Vec2& max)
  , mRequestId(requestId)
  , mSpace(space)
  , mMin(min)
  , mMax(max) {
}

DEFINE_EVENT(ScreenPickResponse, size_t requestId, Handle space, std::vector<Handle>&& objects)
  , mRequestId(requestId)
  , mSpace(space)
  , mObjects(std::move(objects)) {
}