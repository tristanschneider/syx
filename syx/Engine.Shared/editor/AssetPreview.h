#pragma once

class Asset;
class AssetRepo;
class EventHandler;
class MessageQueueProvider;
class PreviewAssetEvent;

class AssetPreview {
public:
  AssetPreview(MessageQueueProvider& msg, EventHandler& handler, AssetRepo& assets);

  void editorUpdate();

private:
  MessageQueueProvider& mMsg;
  AssetRepo& mAssets;
  std::shared_ptr<Asset> mPreview;
};