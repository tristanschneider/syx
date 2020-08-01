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
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"

const std::string LuaGameSystem::INSTANCE_KEY = "LuaGameSystem";
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

  mState = std::make_unique<Lua::State>();
  mLibs = std::make_unique<Lua::AllLuaLibs>();
  _openAllLibs(*mState);

  mComponents = std::make_unique<LuaComponentRegistry>();
  _registerBuiltInComponents();
}

void LuaGameSystem::_openAllLibs(lua_State* l) {
  mLibs->open(l);
  //Store an instance of this in the registry for later
  lua_pushlightuserdata(l, this);
  lua_setfield(l, LUA_REGISTRYINDEX, INSTANCE_KEY.c_str());
}

void LuaGameSystem::queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  CallOnObserversPtr(mSubject, preUpdate, *this);

  mSafeToAccessObjects = false;
  auto events = std::make_shared<FunctionTask>([this]() {
    mEventHandlerThread = std::this_thread::get_id();
    mEventHandler->handleEvents(*mEventBuffer);
    mSafeToAccessObjects = true;
  });

  auto update = std::make_shared<FunctionTask>([this, dt]() {
    _update(dt);
  });

  events->then(update)->then(frameTask);

  pool.queueTask(events);
  pool.queueTask(update);
}

void LuaGameSystem::_update(float dt) {
  Lua::StackAssert sa(*mState);
  for(auto& objIt : mObjects) {
    LuaGameObject::push(*mState, *objIt.second);
    int selfIndex = lua_gettop(*mState);
    dt *= getSpace(objIt.second->getSpace()).getTimescale();
    const bool doUpdate = dt != 0;

    objIt.second->forEachLuaComponent([this, selfIndex, doUpdate, dt](LuaComponent& comp) {
      //If the component needs initialization, get the script and initialize it
      if(comp.needsInit()) {
        AssetRepo* repo = mArgs.mSystems->getSystem<AssetRepo>();
        std::shared_ptr<Asset> script = repo->getAsset(AssetInfo(comp.getScript()));
        //If script isn't done loading, wait until later
        if(!script || script->getState() != AssetState::Loaded)
          return;

        //Load the script on to the top of the stack
        const std::string& scriptSource = static_cast<LuaScript&>(*script).get();
        {
          Lua::StackAssert sa(*mState);
          if(int loadError = luaL_loadstring(*mState, scriptSource.c_str())) {
            printf("Error loading script %s: %s\n", static_cast<LuaScript&>(*script).getInfo().mUri.c_str(), lua_tostring(*mState, -1));
          }
          else {
            comp.init(*mState, selfIndex);
          }
          //Pop off the error or the script
          lua_pop(*mState, 1);
        }
      }
      //Else sandbox is already initialized, do the update
      else if(doUpdate) {
        comp.update(*mState, dt, selfIndex);
      }
    });
    //pop gameobject
    lua_pop(*mState, 1);
  }
}

void LuaGameSystem::_registerBuiltInComponents() {
  auto lock = mComponentsLock.getWriter();
  const auto& ctors = Component::Registry::getConstructors();
  for(auto it = ctors.begin(); it != ctors.end(); ++it) {
    std::unique_ptr<Component> temp = (*it)(0);
    mComponents->registerComponent(temp->getTypeInfo().mTypeName, *it);
  }
}

/*

Component* LuaGameSystem::addComponentFromPropName(const char* name, LuaGameObject& owner) {
  auto lock = mComponentsLock.getReader();
  if(const Component* comp = mComponents->getInstanceByPropName(name))
    return addComponent(comp->getTypeInfo().mTypeName, owner);
  return nullptr;
}

Component* LuaGameSystem::addComponent(const std::string& name, LuaGameObject& owner) {
  std::unique_ptr<Component> component;
  {
    auto lock = mComponentsLock.getReader();
    std::optional<size_t> type = mComponents->getComponentType(name);
    if(!type) {
      return nullptr;
    }
    //Comopnent may already exist, most likely for built in components
    if(Component* existing = owner.getComponent(*type)) {
      return existing;
    }
    component = mComponents->construct(name, owner.getHandle());
    component->setSystem(*this);
  }
  Component* result = component.get();
  if(result) {
    mArgs.mMessages->getMessageQueue().get().push(AddComponentEvent(owner.getHandle(), result->getType()));
    mPendingComponentsLock.lock();
    mPendingComponents.push_back(std::move(component));
    mPendingComponentsLock.unlock();
  }
  return result;
}

void LuaGameSystem::removeComponent(const std::string& name, Handle owner) {
  std::optional<size_t> type;
  {
    auto lock = mComponentsLock.getReader();
    //TODO: Probably need something special here for lua components
    type = mComponents->getComponentType(name);
  }
  if(type)
    mArgs.mMessages->getMessageQueue().get().push(RemoveComponentEvent(owner, *type));
}

void LuaGameSystem::removeComponentFromPropName(const char* name, Handle owner) {
  auto lock = mComponentsLock.getReader();
  if(const Component* comp = mComponents->getInstanceByPropName(name))
    removeComponent(comp->getTypeInfo().mTypeName, owner);
}

*/

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

const LuaComponentRegistry& LuaGameSystem::getComponentRegistry() const {
  return *mComponents;
}

void LuaGameSystem::forEachComponentType(const std::function<void(const Component&)>& callback) const {
  auto lock = mComponentsLock.getReader();
  mComponents->forEachComponent([&callback](const Component& component) {
    callback(component);
  });
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
  mState = nullptr;
}

LuaGameSystem* LuaGameSystem::get(lua_State* l) {
  lua_getfield(l, LUA_REGISTRYINDEX, INSTANCE_KEY.c_str());
  LuaGameSystem* result = static_cast<LuaGameSystem*>(lua_touserdata(l, -1));
  lua_pop(l, 1);
  return result;
}

LuaGameSystem& LuaGameSystem::check(lua_State* l) {
  LuaGameSystem* result = get(l);
  if(!result)
    luaL_error(l, "LuaGameSystem instance didn't exist");
  return *result;
}

void LuaGameSystem::_onAllSystemsInit(const AllSystemsInitialized&) {
}

void LuaGameSystem::_onAddComponent(const AddComponentEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj)) {
    //Don't add types that already exist
    if(obj->getComponent(e.mCompType, e.mSubType))
      return;
    //Try to see if this was a pending component
    std::unique_ptr<Component> pending;
    mPendingComponentsLock.lock();
    for(size_t i = 0; i < mPendingComponents.size(); ++i) {
      std::unique_ptr<Component>& component = mPendingComponents[i];
      if(component->getOwner() == e.mObj && component->getType() == e.mCompType && component->getSubType() == e.mSubType) {
        pending = std::move(component);
        //Erase instead of swap remove as it's very likely order in vector is the order of the messages, so component can often be found at 0 if erased
        mPendingComponents.erase(mPendingComponents.begin() + i);
        break;
      }
    }
    mPendingComponentsLock.unlock();
    if(pending)
      obj->addComponent(std::move(pending));
    else {
      auto comp = Component::Registry::construct(e.mCompType, e.mObj);
      comp->setSubType(e.mSubType);
      obj->addComponent(std::move(comp));
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
  //See if there is a pending object for this message
  std::unique_ptr<LuaGameObject> pending;
  mPendingObjectsLock.lock();
  for(size_t i = 0; i < mPendingObjects.size(); ++i) {
    std::unique_ptr<LuaGameObject>& obj = mPendingObjects[i];
    if(obj->getHandle() == e.mObj) {
      pending = std::move(obj);
      //Erase since order of messages is likely order of pending container
      mPendingObjects.erase(mPendingObjects.begin() + i);
      break;
    }
  }
  mPendingObjectsLock.unlock();
  if(mObjects.find(e.mObj) == mObjects.end())
    mObjects[e.mObj] = pending ? std::move(pending) : std::make_unique<LuaGameObject>(e.mObj);
}

void LuaGameSystem::_onRemoveGameObject(const RemoveGameObjectEvent& e) {
  auto it = mObjects.find(e.mObj);
  if(it != mObjects.end())
    mObjects.erase(it);
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
      LuaGameObject::invalidate(*mState, *it->second);
      it = mObjects.erase(it);
    }
    else
      ++it;
  }

  std::unordered_set<Handle> removed(mPendingObjects.size());
  for(size_t i = 0; i < mPendingObjects.size();) {
    auto& curObj = mPendingObjects[i];
    if(curObj->getSpace() == e.mSpace) {
      LuaGameObject::invalidate(*mState, *curObj);
      removed.insert(curObj->getHandle());
      curObj = std::move(mPendingObjects.back());
      mPendingObjects.pop_back();
    }
  }

  for(size_t i = 0; i < mPendingComponents.size();) {
    auto& curComp = mPendingComponents[i];
    if(removed.find(curComp->getOwner()) != removed.end()) {
      curComp->invalidate(*mState);
      curComp = std::move(mPendingComponents.back());
      mPendingComponents.pop_back();
    }
  }

  auto it = mSpaces.find(e.mSpace);
  if(it != mSpaces.end())
    mSpaces.erase(it);
}

void LuaGameSystem::_onSpaceSave(const SaveSpaceEvent& e) {
  SpaceComponent::_save(*mState, e.mSpace, e.mFile);
}

void LuaGameSystem::_onSpaceLoad(const LoadSpaceEvent& e) {
  SpaceComponent::_load(*mState, e.mSpace, e.mFile);
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