#pragma once
#include "System.h"
#include "provider/MessageQueueProvider.h"
#include "provider/LuaGameObjectProvider.h"

class AddComponentEvent;
class AddGameObjectEvent;
class AddLuaComponentEvent;
class AllSystemsInitialized;
class AssetPreview;
class AssetRepo;
class ClearSpaceEvent;
class Component;
class FilePath;
struct lua_State;
class LuaComponentRegistry;
class LuaGameObject;
class LuaGameSystem;
struct LuaSceneDescription;
class LuaSpace;
class ObjectInspector;
class PhysicsCompUpdateEvent;
class RemoveComponentEvent;
class RemoveGameObjectEvent;
class RemoveLuaComponentEvent;
class RenderableUpdateEvent;
class SceneBrowser;
class ScreenPickResponse;
class SetComponentPropsEvent;
class SpaceComponent;
class Task;
class Toolbox;
class TransformEvent;

namespace Lua {
  class State;
  class LuaLibGroup;
}

class LuaGameSystemObserver {
public:
  virtual ~LuaGameSystemObserver() {}
  //Main thread, safe to access lua objects
  virtual void preUpdate(const LuaGameSystem& game) {}
};
using LuaGameSystemObserverT = Observer<std::unique_ptr<LuaGameSystemObserver>>;

class LuaGameSystem : public System, public LuaGameObjectProvider {
public:
  static const char* CLASS_NAME;

  RegisterSystemH(LuaGameSystem);

  LuaGameSystem(const SystemArgs& args);
  ~LuaGameSystem();

  void init() override;
  void queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

  LuaGameObject* _getObj(Handle h) const;

  static LuaGameSystem* get(lua_State* l);
  static LuaGameSystem& check(lua_State* l);

  //Add component to gameobject with the given owner. Returns a pending component that will be applied next frame. Null if invalid name
  Component* addComponent(const std::string& name, LuaGameObject& owner);
  Component* addComponentFromPropName(const char* name, LuaGameObject& owner);
  void removeComponent(const std::string& name, Handle owner);
  void removeComponentFromPropName(const char* name, Handle owner);
  LuaGameObject& addGameObject();

  void addObserver(LuaGameSystemObserverT& observer);

  MessageQueue getMessageQueue();
  AssetRepo& getAssetRepo();
  const LuaComponentRegistry& getComonentRegistry() const;
  //Safe to access lock free for any task queued as a dependency or dependent of event processing
  const HandleMap<std::unique_ptr<LuaGameObject>>& getObjects() const;
  SpaceComponent& getSpace(Handle id);
  const ProjectLocator& getProjectLocator() const;
  IWorkerPool& getWorkerPool();

  const LuaGameObject* getObject(Handle handle) const;

  void _openAllLibs(lua_State* l);

  static void openLib(lua_State* l);

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
  void _onSpaceClear(const ClearSpaceEvent& e);

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
  //Global instances of each space so spaces can be retreived by name.
  std::unordered_map<Handle, SpaceComponent> mSpaces;
  SpinLock mPendingObjectsLock;
  //Used for debug checking thread safety of public accesses to LuaGameObjects
  bool mSafeToAccessObjects = true;
  LuaGameSystemObserverT::SubjectType mSubject;
};