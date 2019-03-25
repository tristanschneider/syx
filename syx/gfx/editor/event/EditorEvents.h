#include "asset/Asset.h"
#include "event/Event.h"

class SetSelectionEvent : public TypedEvent<SetSelectionEvent> {
public:
  SetSelectionEvent(std::vector<Handle>&& objects);
  std::vector<Handle> mObjects;
};

class ScreenPickResponse : public TypedEvent<ScreenPickResponse> {
public:
  ScreenPickResponse(size_t requestId, Handle space, std::vector<Handle>&& objects);
  size_t mRequestId;
  Handle mSpace;
  std::vector<Handle> mObjects;
};

class ScreenPickRequest : public RequestEvent<ScreenPickRequest, ScreenPickResponse> {
public:
  ScreenPickRequest(size_t requestId, Handle space, const Syx::Vec2& min, const Syx::Vec2& max);
  size_t mRequestId;
  Handle mSpace;
  Syx::Vec2 mMin;
  Syx::Vec2 mMax;
};

class PreviewAssetEvent : public TypedEvent<PreviewAssetEvent> {
public:
  PreviewAssetEvent(AssetInfo asset);
  AssetInfo mAsset;
};

enum class PlayState : uint8_t {
  Invalid,
  Stopped,
  Paused,
  Stepping,
  Playing,
};

class SetPlayStateEvent : public TypedEvent<SetPlayStateEvent> {
public:
  SetPlayStateEvent(PlayState state);
  PlayState mState;
};