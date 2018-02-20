#include "Precompile.h"
#include "system/LuaGameSystem.h"

#include "asset/Asset.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/TransformEvent.h"
#include "LuaGameObject.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/MessageQueueProvider.h"
#include "provider/SystemProvider.h"
#include "system/AssetRepo.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"

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
  SYSTEM_EVENT_HANDLER(AddGameObjectEvent, _onAddGameObject);
  SYSTEM_EVENT_HANDLER(RemoveGameObjectEvent, _onRemoveGameObject);
  SYSTEM_EVENT_HANDLER(RenderableUpdateEvent, _onRenderableUpdate);
  SYSTEM_EVENT_HANDLER(TransformEvent, _onTransformUpdate);
  SYSTEM_EVENT_HANDLER(PhysicsCompUpdateEvent, _onPhysicsUpdate);
}

void LuaGameSystem::queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  auto events = std::make_shared<FunctionTask>([this]() {
    mEventHandler->handleEvents(*mEventBuffer);
  });

  auto update = std::make_shared<FunctionTask>([this]() {
    //TODO: call update on lua scripts
  });
  events->addDependency(update);

  frameTask->addDependency(events);
  pool.queueTask(events);
  pool.queueTask(update);
}

void LuaGameSystem::uninit() {
}

void LuaGameSystem::_onAddComponent(const AddComponentEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj))
    obj->addComponent(Component::Registry::construct(e.mCompType, e.mObj));
}

void LuaGameSystem::_onRemoveComponent(const RemoveComponentEvent& e) {
  if(LuaGameObject* obj = _getObj(e.mObj))
    obj->removeComponent(e.mCompType);
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