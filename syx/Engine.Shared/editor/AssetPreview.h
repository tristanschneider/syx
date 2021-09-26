#pragma once

class Asset;
class EventHandler;
struct EventListener;
class MessageQueueProvider;
class PreviewAssetEvent;

class AssetPreview {
public:
  AssetPreview(MessageQueueProvider& msg, EventHandler& handler);
  ~AssetPreview();

  void editorUpdate();

private:
  std::vector<std::shared_ptr<EventListener>> mListeners;
  MessageQueueProvider& mMsg;
  std::shared_ptr<Asset> mPreview;
};