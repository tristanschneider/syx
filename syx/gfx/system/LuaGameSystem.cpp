#include "Precompile.h"
#include "system/LuaGameSystem.h"

#include "asset/Asset.h"
#include "asset/LuaScript.h"
#include "component/LuaComponent.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/TransformEvent.h"
#include <lua.hpp>
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/AllLuaLibs.h"
#include "LuaGameObject.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/MessageQueueProvider.h"
#include "provider/SystemProvider.h"
#include "system/AssetRepo.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"

const std::string LuaGameSystem::INSTANCE_KEY = "LuaGameSystem";

RegisterSystemCPP(LuaGameSystem);

LuaGameSystem::LuaGameSystem(const SystemArgs& args)
  : System(args) {
}

LuaGameSystem::~LuaGameSystem() {
}

void LuaGameSystem::init() {
  _initHardCodedScene();

  mEventHandler = std::make_unique<EventHandler>();
  SYSTEM_EVENT_HANDLER(AddComponentEvent, _onAddComponent);
  SYSTEM_EVENT_HANDLER(RemoveComponentEvent, _onRemoveComponent);
  SYSTEM_EVENT_HANDLER(AddLuaComponentEvent, _onAddLuaComponent);
  SYSTEM_EVENT_HANDLER(RemoveLuaComponentEvent, _onRemoveLuaComponent)
  SYSTEM_EVENT_HANDLER(AddGameObjectEvent, _onAddGameObject);
  SYSTEM_EVENT_HANDLER(RemoveGameObjectEvent, _onRemoveGameObject);
  SYSTEM_EVENT_HANDLER(RenderableUpdateEvent, _onRenderableUpdate);
  SYSTEM_EVENT_HANDLER(TransformEvent, _onTransformUpdate);
  SYSTEM_EVENT_HANDLER(PhysicsCompUpdateEvent, _onPhysicsUpdate);

  mState = std::make_unique<Lua::State>();
  mLibs = std::make_unique<Lua::AllLuaLibs>();
  mLibs->open(*mState);
  //Store an instance of this in the registry for later
  lua_pushlightuserdata(*mState, this);
  lua_setfield(*mState, LUA_REGISTRYINDEX, INSTANCE_KEY.c_str());
}

void LuaGameSystem::queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  auto events = std::make_shared<FunctionTask>([this]() {
    mEventHandler->handleEvents(*mEventBuffer);
  });

  auto update = std::make_shared<FunctionTask>([this, dt]() {
    _update(dt);
  });
  events->addDependency(update);

  frameTask->addDependency(events);
  pool.queueTask(events);
  pool.queueTask(update);
}

void LuaGameSystem::_update(float dt) {
  for(auto& objIt : mObjects) {
    for(auto& compIt : objIt.second->getLuaComponents()) {
      LuaComponent& comp = compIt.second;
      //If the component needs initialization, get the script and initialize it
      if(comp.needsInit()) {
        AssetRepo* repo = mArgs.mSystems->getSystem<AssetRepo>();
        std::shared_ptr<Asset> script = repo->getAsset(AssetInfo(comp.getScript()));
        assert(script && "Script should exist in repo as creating the component should have triggered the asset load");
        //If script isn't doen loading, wait until later
        if(script->getState() != AssetState::Loaded)
          continue;

        //Load the script on to the top of the stack
        const std::string& scriptSource = static_cast<LuaScript&>(*script).get();
        {
          Lua::StackAssert sa(*mState);
          if(int loadError = luaL_loadstring(*mState, scriptSource.c_str())) {
            printf("Error loading script %s: %s\n", static_cast<LuaScript&>(*script).getInfo().mUri.c_str(), lua_tostring(*mState, -1));
          }
          else {
            comp.init(*mState);
          }
          //Pop off the error or the script
          lua_pop(*mState, 1);
        }
      }
      //Else sandbox is already initialized, do the update
      else {
        comp.update(*mState, dt);
      }
    }
  }
}

void LuaGameSystem::uninit() {
  mObjects.clear();
  mEventHandler = nullptr;
  mState = nullptr;
}

LuaGameSystem* LuaGameSystem::get(lua_State* l) {
  lua_getfield(l, LUA_REGISTRYINDEX, INSTANCE_KEY.c_str());
  return static_cast<LuaGameSystem*>(lua_touserdata(l, -1));
}

void LuaGameSystem::_onAddComponent(const AddComponentEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj))
    obj->addComponent(Component::Registry::construct(e.mCompType, e.mObj));
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
  mObjects[e.mObj] = std::make_unique<LuaGameObject>(e.mObj);
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

LuaGameObject* LuaGameSystem::_getObj(Handle h) {
  auto it = mObjects.find(h);
  return it == mObjects.end() ? nullptr : it->second.get();
}

void LuaGameSystem::_initHardCodedScene() {
  using namespace Syx;
  AssetRepo* repo = mArgs.mSystems->getSystem<AssetRepo>();
  size_t mazeTexId = repo->getAsset(AssetInfo("textures/test.bmp"))->getInfo().mId;

  MessageQueueProvider* msg = mArgs.mMessages;
  {
    Handle h = mArgs.mGameObjectGen->newHandle();
    RenderableData d;
    d.mModel = repo->getAsset(AssetInfo("models/bowserlow.obj"))->getInfo().mId;
    d.mDiffTex = mazeTexId;

    MessageQueue q = msg->getMessageQueue();
    EventBuffer& m = q.get();
    m.push(AddGameObjectEvent(h));
    m.push(AddComponentEvent(h, Component::typeId<Renderable>()));
    m.push(RenderableUpdateEvent(d, h));
    m.push(TransformEvent(h, Syx::Mat4::transform(Vec3(0.1f), Quat::Identity, Vec3::Zero)));
  }

  {
    Handle h = mArgs.mGameObjectGen->newHandle();
    RenderableData d;
    d.mModel = repo->getAsset(AssetInfo("models/car.obj"))->getInfo().mId;
    d.mDiffTex = mazeTexId;

    MessageQueue q = msg->getMessageQueue();
    EventBuffer& m = q.get();
    m.push(AddGameObjectEvent(h));
    m.push(AddComponentEvent(h, Component::typeId<Renderable>()));
    m.push(RenderableUpdateEvent(d, h));
    m.push(TransformEvent(h, Syx::Mat4::transform(Vec3(0.5f), Quat::Identity, Vec3(8.0f, 0.0f, 0.0f))));
  }

  size_t cubeModelId = repo->getAsset(AssetInfo("models/cube.obj"))->getInfo().mId;
  {
    Handle h = mArgs.mGameObjectGen->newHandle();
    RenderableData d;
    d.mModel = cubeModelId;
    d.mDiffTex = mazeTexId;

    Physics phy(h);
    phy.setCollider((*mArgs.mAssetsHack)["pCube"], (*mArgs.mAssetsHack)["pDefMat"]);
    phy.setPhysToModel(Syx::Mat4::scale(Syx::Vec3(2.0f)));

    MessageQueue q = msg->getMessageQueue();
    EventBuffer& m = q.get();
    m.push(AddGameObjectEvent(h));
    m.push(AddComponentEvent(h, Component::typeId<Renderable>()));
    m.push(RenderableUpdateEvent(d, h));
    m.push(AddComponentEvent(h, Component::typeId<Physics>()));
    m.push(PhysicsCompUpdateEvent(phy.getData(), h));
    m.push(TransformEvent(h, Syx::Mat4::transform(Vec3(10.0f, 1.0f, 10.0f), Quat::Identity, Vec3(0.0f, -10.0f, 0.0f))));
    m.push(AddLuaComponentEvent(h, repo->getAsset(AssetInfo("scripts/test.lc"))->getInfo().mId));
  }

  {
    Handle h = mArgs.mGameObjectGen->newHandle();
    RenderableData d;
    d.mModel = cubeModelId;
    d.mDiffTex = mazeTexId;

    Physics phy(h);
    phy.setCollider((*mArgs.mAssetsHack)["pCube"], (*mArgs.mAssetsHack)["pDefMat"]);
    phy.setPhysToModel(Syx::Mat4::scale(Syx::Vec3(2.0f)));
    phy.setRigidbody(Syx::Vec3::Zero, Syx::Vec3::Zero);
    phy.setAngVel(Vec3(3.0f));

    MessageQueue q = msg->getMessageQueue();
    EventBuffer& m = q.get();
    m.push(AddGameObjectEvent(h));
    m.push(AddComponentEvent(h, Component::typeId<Renderable>()));
    m.push(RenderableUpdateEvent(d, h));
    m.push(AddComponentEvent(h, Component::typeId<Physics>()));
    m.push(PhysicsCompUpdateEvent(phy.getData(), h));
    m.push(TransformEvent(h, Syx::Mat4::transform(Vec3(1.0f, 1.0f, 1.0f), Quat::Identity, Vec3(0.0f, 8.0f, 0.0f))));
  }
}