#pragma once
#include "System.h"

class Task;
class LuaGameObject;

class AddComponentEvent;
class AddGameObjectEvent;
class RemoveComponentEvent;
class RemoveGameObjectEvent;
class RenderableUpdateEvent;
class TransformEvent;
class PhysicsCompUpdateEvent;

class LuaGameSystem : public System {
public:
  RegisterSystemH(LuaGameSystem);
  LuaGameSystem(const SystemArgs& args);
  ~LuaGameSystem();

  void init() override;
  void queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

private:
  //TODO: make it possible to do this from lua
  void _initHardCodedScene();
  LuaGameObject* _getObj(Handle h);

  void _onAddComponent(const AddComponentEvent& e);
  void _onRemoveComponent(const RemoveComponentEvent& e);
  void _onAddGameObject(const AddGameObjectEvent& e);
  void _onRemoveGameObject(const RemoveGameObjectEvent& e);
  void _onRenderableUpdate(const RenderableUpdateEvent& e);
  void _onTransformUpdate(const TransformEvent& e);
  void _onPhysicsUpdate(const PhysicsCompUpdateEvent& e);

  HandleMap<std::unique_ptr<LuaGameObject>> mObjects;
};