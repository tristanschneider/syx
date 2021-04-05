#pragma once
#include "System.h"
#include "provider/ComponentRegistryProvider.h"
#include "provider/MessageQueueProvider.h"
#include "provider/LuaGameObjectProvider.h"
#include "threading/SpinLock.h"
#include "util/Observer.h"

class AddComponentEvent;
class AddGameObjectEvent;
class AddLuaComponentEvent;
class AllSystemsInitialized;
class AssetPreview;
class AssetRepo;
class ClearSpaceEvent;
class Component;
struct ComponentDataRequest;
class ComponentRegistryProvider;
class FilePath;
class GameObjectHandleProvider;
struct IIDRegistry;
struct ILuaGameContext;
class LoadSpaceEvent;
class LuaGameObject;
class LuaGameSystem;
class LuaSpace;
class ObjectInspector;
class PhysicsCompUpdateEvent;
class RemoveComponentEvent;
class RemoveGameObjectEvent;
class RemoveLuaComponentEvent;
class RenderableUpdateEvent;
class SaveSpaceEvent;
class SceneBrowser;
class ScreenPickResponse;
class SetComponentPropsEvent;
class SetTimescaleEvent;
class Space;
class Task;
class Toolbox;
class TransformEvent;

struct LuaSceneDescription;
struct lua_State;

namespace Lua {
  class State;
  class LuaLibGroup;
}

namespace FileSystem {
  struct IFileSystem;
}

class LuaGameSystemObserver : public Observer<LuaGameSystemObserver> {
public:
  virtual ~LuaGameSystemObserver() {}
  //Main thread, safe to access lua objects
  virtual void preUpdate(const LuaGameSystem&) {}
  virtual void onObjectDestroyed(const LuaGameObject&) {}
};

// This class is responsible for owning and mutating state on LuaGameObjects, as well as owning the LuaGameContexts.
// The contexts are what are used to dole out the work of updating the gameobjects and sending messages to request mutations.
// Pending state is tracked locally within the context until the messages are processed by the system, who is the source of truth
class LuaGameSystem
  : public System
  , public LuaGameObjectProvider {
public:
  static const char* CLASS_NAME;

  LuaGameSystem(const SystemArgs& args);
  ~LuaGameSystem();

  void init() override;
  void queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

  LuaGameObject* _getObj(Handle h) const;

  void addObserver(LuaGameSystemObserver& observer);

  MessageQueue getMessageQueue();
  MessageQueueProvider& getMessageQueueProvider();
  AssetRepo& getAssetRepo();
  ComponentRegistryProvider& getComponentRegistry() const;
  //Safe to access lock free for any task queued as a dependency or dependent of event processing
  const HandleMap<std::shared_ptr<LuaGameObject>>& getObjects() const;
  Space& getSpace(Handle id);
  const ProjectLocator& getProjectLocator() const;
  IWorkerPool& getWorkerPool();
  GameObjectHandleProvider& getGameObjectGen() const;
  IIDRegistry& getIDRegistry() const;
  FileSystem::IFileSystem& getFileSystem();

  const LuaGameObject* getObject(Handle handle) const;

  void _openAllLibs(lua_State* l);

  static void openLib(lua_State* l);

private:

  std::unique_ptr<ILuaGameContext> _createGameContext();
  ILuaGameContext& _getNextGameContext();

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
  void _onSpaceSave(const SaveSpaceEvent& e);
  void _onSpaceLoad(const LoadSpaceEvent& e);
  void _onSetTimescale(const SetTimescaleEvent& e);
  void _onComponentDataRequest(const ComponentDataRequest& e);

  HandleMap<std::shared_ptr<LuaGameObject>> mObjects;
  std::unique_ptr<Lua::LuaLibGroup> mLibs;
  // TODO: I don't think this lock is necessary
  mutable RWLock mComponentsLock;
  std::unordered_map<Handle, Space> mSpaces;
  SpinLock mPendingObjectsLock;
  //Used for debug checking thread safety of public accesses to LuaGameObjects
  bool mSafeToAccessObjects = true;
  std::thread::id mEventHandlerThread;
  LuaGameSystemObserver::SubjectType mSubject;
  std::vector<std::unique_ptr<ILuaGameContext>> mLuaContexts;
};
