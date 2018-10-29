#include "asset/Asset.h"
#include "event/Event.h"

class SetSelectionEvent : public Event {
public:
  SetSelectionEvent(std::vector<Handle>&& objects);
  std::vector<Handle> mObjects;
};

class ScreenPickRequest : public Event {
public:
  ScreenPickRequest(size_t requestId, Handle space, const Syx::Vec2& min, const Syx::Vec2& max);
  size_t mRequestId;
  Handle mSpace;
  Syx::Vec2 mMin;
  Syx::Vec2 mMax;
};

class ScreenPickResponse : public Event {
public:
  ScreenPickResponse(size_t requestId, Handle space, std::vector<Handle>&& objects);
  size_t mRequestId;
  Handle mSpace;
  std::vector<Handle> mObjects;
};

class PreviewAssetEvent : public Event {
public:
  PreviewAssetEvent(AssetInfo asset);
  AssetInfo mAsset;
};