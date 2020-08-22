#include "Precompile.h"
#include "system/LuaGameSystem.h"

#include "asset/Asset.h"
#include "asset/LuaScript.h"
#include "component/LuaComponent.h"
#include "component/LuaComponentRegistry.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/LifecycleEvents.h"
#include "event/SpaceEvents.h"
#include "event/TransformEvent.h"
#include <lua.hpp>
#include "lua/AllLuaLibs.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaNode.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/LuaUtil.h"
#include "LuaGameObject.h"
#include "ProjectLocator.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/MessageQueueProvider.h"
#include "provider/SystemProvider.h"
#include "Space.h"
#include "system/AssetRepo.h"
#include "threading/AsyncHandle.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"

const char* LuaGameSystem::CLASS_NAME = "Game";

LuaGameSystem::LuaGameSystem(const SystemArgs& args)
  : System(args) {
}

LuaGameSystem::~LuaGameSystem() {
}

void LuaGameSystem::init() {
  mEventHandler = std::make_unique<EventHandler>();
  SYSTEM_EVENT_HANDLER(AddComponentEvent, _onAddComponent);
  SYSTEM_EVENT_HANDLER(RemoveComponentEvent, _onRemoveComponent);
  //TODO: can this be removed? I think addcomponent does the same thing
  SYSTEM_EVENT_HANDLER(AddLuaComponentEvent, _onAddLuaComponent);
  SYSTEM_EVENT_HANDLER(RemoveLuaComponentEvent, _onRemoveLuaComponent)
  SYSTEM_EVENT_HANDLER(AddGameObjectEvent, _onAddGameObject);
  SYSTEM_EVENT_HANDLER(RemoveGameObjectEvent, _onRemoveGameObject);
  SYSTEM_EVENT_HANDLER(RenderableUpdateEvent, _onRenderableUpdate);
  SYSTEM_EVENT_HANDLER(TransformEvent, _onTransformUpdate);
  SYSTEM_EVENT_HANDLER(PhysicsCompUpdateEvent, _onPhysicsUpdate);
  SYSTEM_EVENT_HANDLER(SetComponentPropsEvent, _onSetComponentProps);
  SYSTEM_EVENT_HANDLER(AllSystemsInitialized, _onAllSystemsInit);
  SYSTEM_EVENT_HANDLER(ClearSpaceEvent, _onSpaceClear);
  SYSTEM_EVENT_HANDLER(SaveSpaceEvent, _onSpaceSave);
  SYSTEM_EVENT_HANDLER(LoadSpaceEvent, _onSpaceLoad);
  SYSTEM_EVENT_HANDLER(SetTimescaleEvent, _onSetTimescale);
  mEventHandler->registerEventHandler<CallbackEvent>(CallbackEvent::getHandler(typeId<LuaGameSystem>()));

  mLibs = std::make_unique<Lua::AllLuaLibs>();

  //Arbitrarily create a static amount of contexts to load balance with
  constexpr size_t numContexts = 4;
  mLuaContexts.reserve(numContexts);
  for(size_t i = 0; i < 4; ++i) {
    mLuaContexts.emplace_back(_createGameContext());
  }
}

void LuaGameSystem::_openAllLibs(lua_State* l) {
  mLibs->open(l);
  auto registry = mArgs.mComponentRegistry->getReader();
  registry.first.forEachComponent([l](const Component& component) {
    component.openLib(l);
  });
}

void LuaGameSystem::queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  mSubject.dispatch(&LuaGameSystemObserver::preUpdate, *this);

  //First process all events, which will apply the changes from last frame
  mSafeToAccessObjects = false;
  auto events = std::make_shared<FunctionTask>([this]() {
    mEventHandlerThread = std::this_thread::get_id();
    mEventHandler->handleEvents(*mEventBuffer);
    mSafeToAccessObjects = true;
  });
  pool.queueTask(events);

  //Queue a task for each context to update itself after event processing
  for(auto& luaContext : mLuaContexts) {
    auto update = std::make_shared<FunctionTask>([&luaContext, dt]() {
      luaContext->update(dt);
    });
    events->then(update)->then(frameTask);
    pool.queueTask(update);
  }
}

std::unique_ptr<ILuaGameContext> LuaGameSystem::_createGameContext() {
  auto luaState = std::make_unique<Lua::State>();
  _openAllLibs(*luaState);
  return Lua::createGameContext(*this, std::move(luaState));
}

ILuaGameContext& LuaGameSystem::_getNextGameContext() {
  //Round robin based on number of objects Assuming this is called for the purpose of adding an object, this will divide the count among them evenly
  return *mLuaContexts[mObjects.size() % mLuaContexts.size()];
}

void LuaGameSystem::addObserver(LuaGameSystemObserver& observer) {
  observer.observe(&mSubject);
}

MessageQueue LuaGameSystem::getMessageQueue() {
  return mArgs.mMessages->getMessageQueue();
}

MessageQueueProvider& LuaGameSystem::getMessageQueueProvider() {
  return *mArgs.mMessages;
}

AssetRepo& LuaGameSystem::getAssetRepo() {
  return *mArgs.mSystems->getSystem<AssetRepo>();
}

ComponentRegistryProvider& LuaGameSystem::getComponentRegistry() const {
  return *mArgs.mComponentRegistry;
}

const HandleMap<std::shared_ptr<LuaGameObject>>& LuaGameSystem::getObjects() const {
  assert((mSafeToAccessObjects || mEventHandlerThread == std::this_thread::get_id()) && "Lua objects should only be accessed on tasks depending on event processing");
  return mObjects;
}

Space& LuaGameSystem::getSpace(Handle id) {
  auto it = mSpaces.find(id);
  if(it != mSpaces.end())
    return it->second;
  return mSpaces.emplace(id, id).first->second;
}

const ProjectLocator& LuaGameSystem::getProjectLocator() const {
  return *mArgs.mProjectLocator;
}

IWorkerPool& LuaGameSystem::getWorkerPool() {
  return *mArgs.mPool;
}

GameObjectHandleProvider& LuaGameSystem::getGameObjectGen() const {
  return *mArgs.mGameObjectGen;
}

const LuaGameObject* LuaGameSystem::getObject(Handle handle) const {
  assert((mSafeToAccessObjects || mEventHandlerThread == std::this_thread::get_id()) && "Lua objects should only be accessed on tasks depending on event processing");
  return _getObj(handle);
}

void LuaGameSystem::uninit() {
  mObjects.clear();
  mEventHandler = nullptr;
}

void LuaGameSystem::_onAllSystemsInit(const AllSystemsInitialized&) {
}

void LuaGameSystem::_onAddComponent(const AddComponentEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj)) {
    //Don't add types that already exist
    if(obj->getComponent(e.mCompType, e.mSubType)) {
      return;
    }

    // Add new component
    auto components = mArgs.mComponentRegistry->getReader();
    //components.first.
    auto comp = mArgs.mComponentRegistry->getReader().first.construct(ComponentType{ e.mCompType, e.mSubType }, e.mObj);
    comp->setSubType(e.mSubType);
    obj->addComponent(std::move(comp));
  }
}

void LuaGameSystem::_onRemoveComponent(const RemoveComponentEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj))
    obj->removeComponent(e.mCompType);
}

void LuaGameSystem::_onAddLuaComponent(const AddLuaComponentEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mOwner))
    obj->addLuaComponent(e.mScript);
}

void LuaGameSystem::_onRemoveLuaComponent(const RemoveLuaComponentEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mOwner))
    obj->removeLuaComponent(e.mScript);
}

void LuaGameSystem::_onAddGameObject(const AddGameObjectEvent& e) {
  mArgs.mGameObjectGen->blacklistHandle(e.mObj);
  if(mObjects.find(e.mObj) == mObjects.end()) {
    auto newObj = std::make_shared<LuaGameObject>(e.mObj);
    mObjects[e.mObj] = newObj;
    _getNextGameContext().addObject(newObj);
  }
}

void LuaGameSystem::_onRemoveGameObject(const RemoveGameObjectEvent& e) {
  auto it = mObjects.find(e.mObj);
  if(it != mObjects.end()) {
    mSubject.dispatch(&LuaGameSystemObserver::onObjectDestroyed, *it->second);
    mObjects.erase(it);
  }
}

void LuaGameSystem::_onRenderableUpdate(const RenderableUpdateEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj)) {
    if(Renderable* c = obj->getComponent<Renderable>()) {
      c->set(e.mData);
    }
  }
}

void LuaGameSystem::_onTransformUpdate(const TransformEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mHandle))
    obj->getTransform().set(e.mTransform);
}

void LuaGameSystem::_onPhysicsUpdate(const PhysicsCompUpdateEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mOwner)) {
    if(Physics* c = obj->getComponent<Physics>()) {
      c->setData(e.mData);
    }
  }
}

void LuaGameSystem::_onSetComponentProps(const SetComponentPropsEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj)) {
    if(Component* comp = obj->getComponent(e.mCompType)) {
      e.mProp->copyFromBuffer(comp, e.mBuffer.data(), e.mDiff);
      comp->onPropsUpdated();
    }
  }
}

void LuaGameSystem::_onSpaceClear(const ClearSpaceEvent& e) {
  for(auto it = mObjects.begin(); it != mObjects.end();) {
    if(it->second->getSpace() == e.mSpace) {
      mSubject.dispatch(&LuaGameSystemObserver::onObjectDestroyed, *it->second);

      it = mObjects.erase(it);
    }
    else
      ++it;
  }

  if(auto it = mSpaces.find(e.mSpace); it != mSpaces.end()) {
    mSpaces.erase(it);
  }
}

void LuaGameSystem::_onSpaceSave(const SaveSpaceEvent& e) {
  auto context = _createGameContext();
  SpaceComponent::_save(context->getLuaState(), e.mSpace, e.mFile);
}

void LuaGameSystem::_onSpaceLoad(const LoadSpaceEvent& e) {
  // Keep the context alive until the task completes
  std::shared_ptr<ILuaGameContext> context = _createGameContext();
  SpaceComponent::_load(context->getLuaState(), e.mSpace, e.mFile)->then([context](auto&&) {});
}

void LuaGameSystem::_onSetTimescale(const SetTimescaleEvent& e) {
  getSpace(e.mSpace).setTimescale(e.mTimescale);
}

LuaGameObject* LuaGameSystem::_getObj(Handle h) const {
  auto it = mObjects.find(h);
  return it == mObjects.end() ? nullptr : it->second.get();
}

void LuaGameSystem::openLib(lua_State* l) {
  luaL_Reg statics[] = {
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, CLASS_NAME);
}