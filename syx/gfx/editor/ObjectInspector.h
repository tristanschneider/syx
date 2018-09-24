#pragma once

class EventHandler;
class LuaGameObject;
class LuaGameObjectProvider;
class MessageQueueProvider;
class SetSelectionEvent;

namespace Lua {
  class Variant;
}

class ObjectInspector {
public:
  ObjectInspector(MessageQueueProvider& msg, EventHandler& handler);

  void editorUpdate(const LuaGameObjectProvider& objects);

private:
  void _updateSelection(const LuaGameObjectProvider& objects);

  MessageQueueProvider& mMsg;
  std::vector<Handle> mSelected;
  std::vector<Handle> mPrevSelected;
  std::vector<std::unique_ptr<LuaGameObject>> mSelectedData;
};