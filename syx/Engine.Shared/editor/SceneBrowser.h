#pragma once

class EventHandler;
class GameObjectHandleProvider;
class KeyboardInput;
class LuaGameObject;
class MessageQueueProvider;
class ScreenPickResponse;

class SceneBrowser {
public:
  SceneBrowser(MessageQueueProvider& msg, GameObjectHandleProvider& handleGen, KeyboardInput& input, EventHandler& handler);

  void editorUpdate(const HandleMap<std::shared_ptr<LuaGameObject>>& objects);

  static const char* WINDOW_NAME;
  static const char* NEW_OBJECT_LABEL;
  static const char* DELETE_OBJECT_LABEL;
  static const char* OBJECT_LIST_NAME;

private:
  void _updatePick();
  //Clear current selection to make room for a new one unless the current selection should be added to
  void _clearForNewSelection();
  void _drawSelected();
  void _broadcastSelection() const;

  MessageQueueProvider* mMsg;
  GameObjectHandleProvider* mHandleGen;
  KeyboardInput* mInput;
  std::unordered_set<Handle> mSelected;
  Syx::Vec2 mMouseDownPos;
};