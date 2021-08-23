#pragma once

class Asset;
class AssetRepo;
class EventHandler;
struct EventListener;
class MessageQueueProvider;
class PreviewAssetEvent;

class AssetPreview {
public:
  AssetPreview(MessageQueueProvider& msg, EventHandler& handler, AssetRepo& assets);
  ~AssetPreview();

  void editorUpdate();

private:
  std::vector<std::shared_ptr<EventListener>> mListeners;
  MessageQueueProvider& mMsg;
  AssetRepo& mAssets;
  std::shared_ptr<Asset> mPreview;
};