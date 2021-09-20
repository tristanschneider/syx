#pragma once

class EventHandler;
struct EventListener;
class GameObjectHandleProvider;
class InputStore;
class LuaGameObject;
class MessageQueueProvider;
class ScreenPickResponse;

class SceneBrowser {
public:
  SceneBrowser(MessageQueueProvider& msg, GameObjectHandleProvider& handleGen, std::shared_ptr<InputStore> input, EventHandler& handler);

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

  std::vector<std::shared_ptr<EventListener>> mListeners;
  MessageQueueProvider* mMsg;
  GameObjectHandleProvider* mHandleGen;
  std::shared_ptr<InputStore> mInput;
  std::unordered_set<Handle> mSelected;
  Syx::Vec2 mMouseDownPos;
};