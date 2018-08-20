#pragma once

class GameObjectHandleProvider;
class LuaGameObject;
class MessageQueueProvider;

class SceneBrowser {
public:
  SceneBrowser(MessageQueueProvider* msg, GameObjectHandleProvider* handleGen);

  void editorUpdate(const HandleMap<std::unique_ptr<LuaGameObject>>& objects);

private:
  MessageQueueProvider* mMsg;
  GameObjectHandleProvider* mHandleGen;
  Handle mSelected;
};