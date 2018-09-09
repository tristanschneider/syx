#pragma once

class GameObjectHandleProvider;
class LuaGameObject;
class MessageQueueProvider;
class KeyboardInput;
class ScreenPickResponse;

class SceneBrowser {
public:
  SceneBrowser(MessageQueueProvider* msg, GameObjectHandleProvider* handleGen, KeyboardInput* input);

  void editorUpdate(const HandleMap<std::unique_ptr<LuaGameObject>>& objects);
  void onPickResponse(const ScreenPickResponse& response);

private:
  void _updatePick();
  //Clear current selection to make room for a new one unless the current selection should be added to
  void _clearForNewSelection();
  void _drawSelected();

  MessageQueueProvider* mMsg;
  GameObjectHandleProvider* mHandleGen;
  KeyboardInput* mInput;
  std::unordered_set<Handle> mSelected;
  Syx::Vec2 mMouseDownPos;
};