#include "Precompile.h"
#include "system/LuaGameSystem.h"

#include "asset/Asset.h"
#include "asset/LuaScript.h"
#include "component/LuaComponent.h"
#include "component/LuaComponentRegistry.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "event/BaseComponentEvents.h"
#include "event/DeferredEventBuffer.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/GameEvents.h"
#include "event/LifecycleEvents.h"
#include "event/SpaceEvents.h"
#include "event/TransformEvent.h"
#include "input/InputStore.h"
#include <lua.hpp>
#include "lua/AllLuaLibs.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaNode.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/LuaUtil.h"
#include "LuaGameObject.h"
#include "Macros.h"
#include "ProjectLocator.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/MessageQueueProvider.h"
#include "provider/SystemProvider.h"
#include "registry/IDRegistry.h"
#include "Space.h"
#include "threading/AsyncHandle.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"

const char* LuaGameSystem::CLASS_NAME = "Game";

LuaGameSystem::LuaGameSystem(const SystemArgs& args)
  : System(args, _typeId<LuaGameSystem>()) {
}

LuaGameSystem::~LuaGameSystem() = default;

void LuaGameSystem::init() {
  mEventHandler = std::make_unique<EventHandler>();
  _registerSystemEventHandler(&LuaGameSystem::_onAddComponent);
  _registerSystemEventHandler(&LuaGameSystem::_onRemoveComponent);
  //TODO: can this be removed? I think addcomponent does the same thing
  _registerSystemEventHandler(&LuaGameSystem::_onAddLuaComponent);
  _registerSystemEventHandler(&LuaGameSystem::_onRemoveLuaComponent);
  _registerSystemEventHandler(&LuaGameSystem::_onAddGameObject);
  _registerSystemEventHandler(&LuaGameSystem::_onRemoveGameObject);
  _registerSystemEventHandler(&LuaGameSystem::_onRenderableUpdate);
  _registerSystemEventHandler(&LuaGameSystem::_onTransformUpdate);
  _registerSystemEventHandler(&LuaGameSystem::_onSetComponentProps);
  _registerSystemEventHandler(&LuaGameSystem::_onAllSystemsInit);
  _registerSystemEventHandler(&LuaGameSystem::_onSpaceClear);
  _registerSystemEventHandler(&LuaGameSystem::_onSpaceSave);
  _registerSystemEventHandler(&LuaGameSystem::_onSpaceLoad);
  _registerSystemEventHandler(&LuaGameSystem::_onSetTimescale);
  _registerSystemEventHandler(&LuaGameSystem::_onAddGameObserver);

  _registerCallbackEventHandler(*this);
  _registerSystemEventHandler(&LuaGameSystem::_onComponentDataRequest);

  mLibs = std::make_unique<Lua::AllLuaLibs>();

  mInput = std::make_shared<InputStore>();
  mInput->init(*mEventHandler);

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
  return Lua::createGameContext(*this);
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

IIDRegistry& LuaGameSystem::getIDRegistry() const {
  return *mArgs.mIDRegistry;
}

FileSystem::IFileSystem& LuaGameSystem::getFileSystem() {
  return *mArgs.mFileSystem;
}

const InputStore& LuaGameSystem::getInput() const {
  return *mInput;
}

const LuaGameObject* LuaGameSystem::getObject(Handle handle) const {
  assert((mSafeToAccessObjects || mEventHandlerThread == std::this_thread::get_id()) && "Lua objects should only be accessed on tasks depending on event processing");
  return _getObj(handle);
}

void LuaGameSystem::uninit() {
  mObjects.clear();
  mLuaContexts.clear();
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
    if(auto comp = mArgs.mComponentRegistry->getReader().first.construct(ComponentType{ e.mCompType, e.mSubType }, e.mObj)) {
      comp->setSubType(e.mSubType);
      obj->addComponent(std::move(comp));
    }
    else {
      DEBUG_LOG("Unknown component type in _onAddComponent");
    }
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
    auto newObj = std::make_shared<LuaGameObject>(e.mObj, e.mUniqueID ? e.mUniqueID : std::shared_ptr<IClaimedUniqueID>(mArgs.mIDRegistry->generateNewUniqueID()) );
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

void LuaGameSystem::_onSetComponentProps(const SetComponentPropsEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj)) {
    if(Component* comp = obj->getComponent(e.mCompType)) {
      e.mProp->copyFromBuffer(comp, e.mBuffer.data(), e.mDiff);
      comp->onPropsUpdated();
    }
  }
}

void LuaGameSystem::_onSpaceClear(const ClearSpaceEvent& e) {
  bool anyRemoved = false;
  for(auto it = mObjects.begin(); it != mObjects.end();) {
    if(it->second->getSpace() == e.mSpace) {
      mSubject.dispatch(&LuaGameSystemObserver::onObjectDestroyed, *it->second);

      it = mObjects.erase(it);
      anyRemoved = true;
    }
    else
      ++it;
  }

  if(auto it = mSpaces.find(e.mSpace); it != mSpaces.end()) {
    mSpaces.erase(it);
  }

  if(anyRemoved) {
    for(auto& context : mLuaContexts) {
      context->clearCache();
    }
  }
}

void LuaGameSystem::_onSpaceSave(const SaveSpaceEvent& e) {
  std::shared_ptr<ILuaGameContext> context = _createGameContext();
  // Keep the context alive
  SpaceComponent::_save(context->getLuaState(), e.mSpace, e.mFile)->then([context](IAsyncHandle<bool>&) {});
}

void LuaGameSystem::_onSpaceLoad(const LoadSpaceEvent& e) {
  // Keep the context alive until the task completes
  std::shared_ptr<ILuaGameContext> context = _createGameContext();
  SpaceComponent::_load(context->getLuaState(), e.mSpace, e.mFile)->then([context, e, this](IAsyncHandle<bool>& result) {
    //Send an event to ourselves after a tick to make sure that the events caused by loading the scene have been processed
    CallbackEvent response(_typeId<LuaGameSystem>(), [this, e, result(*result.getResult())] {
      e.respond(*getMessageQueueProvider().getMessageQueue(), result);
    });

    getMessageQueueProvider().getDeferredMessageQueue()->push(std::move(response), DeferredEventBuffer::Condition::waitTicks(1));
  });
}

void LuaGameSystem::_onSetTimescale(const SetTimescaleEvent& e) {
  getSpace(e.mSpace).setTimescale(e.mTimescale);
}

void LuaGameSystem::_onComponentDataRequest(const ComponentDataRequest& e) {
  std::vector<uint8_t> result;
  if(const LuaGameObject* obj = _getObj(e.mObj)) {
    if(const Component* component = obj->getComponent(e.mType)) {
      if(const Lua::Node* props = component->getLuaProps()) {
        result.resize(props->size());
        props->copyToBuffer(component, result.data());
      }
    }
  }

  e.respond(*getMessageQueue(), ComponentDataResponse{ std::move(result) });
}

void LuaGameSystem::_onAddGameObserver(const AddGameObserver& e) {
  if(e.mObserver) {
    addObserver(*e.mObserver);
  }
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