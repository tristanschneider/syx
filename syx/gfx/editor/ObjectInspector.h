#pragma once

class Component;
class ComponentRegistryProvider;
class DefaultInspectors;
class EventHandler;
class LuaGameObject;
class LuaGameObjectProvider;
class MessageQueueProvider;
class SetSelectionEvent;

namespace Lua {
  class Node;
  class Variant;
}

class ObjectInspector {
public:
  ObjectInspector(MessageQueueProvider& msg, EventHandler& handler, const ComponentRegistryProvider& componentRegistry);
  ~ObjectInspector();

  void editorUpdate(const LuaGameObjectProvider& objects);

private:
  void _updateSelection(const LuaGameObjectProvider& objects);
  bool _inspectProperty(const Lua::Node& prop, void* data) const;
  void _showComponentPicker() const;
  void _deleteComponentButton(const Component& component);

  MessageQueueProvider& mMsg;
  std::vector<Handle> mSelected;
  std::vector<Handle> mPrevSelected;
  std::vector<std::unique_ptr<LuaGameObject>> mSelectedData;
  std::unique_ptr<DefaultInspectors> mDefaultInspectors;
  const ComponentRegistryProvider& mComponentRegistry;
};