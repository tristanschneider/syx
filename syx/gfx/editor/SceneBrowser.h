#pragma once

class MessageQueueProvider;
class LuaGameObject;

class SceneBrowser {
public:
  SceneBrowser(MessageQueueProvider* msg);

  void editorUpdate(const HandleMap<std::unique_ptr<LuaGameObject>>& objects);

private:
  MessageQueueProvider* mMsg;
};