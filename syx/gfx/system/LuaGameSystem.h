#pragma once
#include "System.h"
#include "provider/MessageQueueProvider.h"

class AssetRepo;
class Task;
class LuaGameObject;

class AddComponentEvent;
class AddLuaComponentEvent;
class AddGameObjectEvent;
class ClearSceneEvent;
class Component;
class FilePath;
class LuaComponentRegistry;
struct LuaSceneDescription;
class LuaSpace;
class RemoveComponentEvent;
class RemoveGameObjectEvent;
class RenderableUpdateEvent;
class TransformEvent;
class PhysicsCompUpdateEvent;
class AddLuaComponentEvent;
class RemoveLuaComponentEvent;
class SetComponentPropsEvent;
class AllSystemsInitialized;
struct lua_State;

namespace Lua {
  class State;
  class LuaLibGroup;
}

class LuaGameSystem : public System {
public:
  static const char* CLASS_NAME;

  RegisterSystemH(LuaGameSystem);

  LuaGameSystem(const SystemArgs& args);
  ~LuaGameSystem();

  void init() override;
  void queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

  LuaGameObject* _getObj(Handle h);

  static LuaGameSystem* get(lua_State* l);
  static LuaGameSystem& check(lua_State* l);

  //Add component to gameobject with the given owner. Returns a pending component that will be applied next frame. Null if invalid name
  Component* addComponent(const std::string& name, Handle owner);
  void removeComponent(const std::string& name, Handle owner);
  LuaGameObject& addGameObject();

  void cloneScene(Handle fromScene, Handle toScene);
  void clearScene(Handle sceneId);
  void addObjectsFromScene(Handle fromScene, Handle toScene);

  MessageQueue getMessageQueue();
  AssetRepo& getAssetRepo();
  const LuaComponentRegistry& getComonentRegistry() const;
  LuaSpace& getSpace(Handle id);

  static void openLib(lua_State* l);
  //void cloneScene(Scene from, Scene to)
  static int _cloneScene(lua_State* l);
  //void saveScene(Scene scene, string filename)
  static int _saveScene(lua_State* l);
  //returns true if scene exists
  //bool loadScene(Scene scene, string filename)
  static int _loadScene(lua_State* l);

private:

  void _registerBuiltInComponents();
  void _update(float dt);

  //TODO: make it possible to do this from lua
  void _initHardCodedScene();

  void _onAllSystemsInit(const AllSystemsInitialized& e);
  void _onAddComponent(const AddComponentEvent& e);
  void _onRemoveComponent(const RemoveComponentEvent& e);
  void _onAddLuaComponent(const AddLuaComponentEvent& e);
  void _onRemoveLuaComponent(const RemoveLuaComponentEvent& e);
  void _onAddGameObject(const AddGameObjectEvent& e);
  void _onRemoveGameObject(const RemoveGameObjectEvent& e);
  void _onRenderableUpdate(const RenderableUpdateEvent& e);
  void _onTransformUpdate(const TransformEvent& e);
  void _onPhysicsUpdate(const PhysicsCompUpdateEvent& e);
  void _onSetComponentProps(const SetComponentPropsEvent& e);
  void _onSceneClear(const ClearSceneEvent& e);

  void _loadSceneFromDescription(const LuaSceneDescription& scene);

  FilePath _sceneNameToFullPath(const char* scene) const;
  void _openAllLibs(lua_State* l);

  static const std::string INSTANCE_KEY;

  HandleMap<std::unique_ptr<LuaGameObject>> mObjects;
  std::unique_ptr<Lua::State> mState;
  std::unique_ptr<Lua::LuaLibGroup> mLibs;
  std::unique_ptr<LuaComponentRegistry> mComponents;
  RWLock mComponentsLock;
  //Pending objects that have been created this frame. For said frame they are available only to the caller that created them.
  //Next frame they are moved to the system and available to all
  std::vector<std::unique_ptr<Component>> mPendingComponents;
  SpinLock mPendingComponentsLock;
  std::vector<std::unique_ptr<LuaGameObject>> mPendingObjects;
  std::unordered_map<Handle, LuaSpace> mSpaces;
  SpinLock mPendingObjectsLock;
};