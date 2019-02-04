#include "Precompile.h"
#include "system/PhysicsSystem.h"

#include "asset/Model.h"
#include "asset/PhysicsModel.h"
#include "system/AssetRepo.h"
#include "system/GraphicsSystem.h"
#include "event/BaseComponentEvents.h"
#include "event/Event.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/SpaceEvents.h"
#include "component/Physics.h"
#include "component/Transform.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"
#include "event/TransformEvent.h"
#include "provider/SystemProvider.h"
#include "provider/MessageQueueProvider.h"
#include "lua/LuaNode.h"

#include <SyxIntrusive.h>
#include <SyxHandles.h>
#include <SyxHandleMap.h>
#include <SyxPhysicsSystem.h>
#include <SyxModelParam.h>

#include <SyxIslandTests.h>

namespace Syx {
  namespace Interface {
    extern ::DebugDrawer* gDrawer;
  }
}

RegisterSystemCPP(PhysicsSystem);

const std::string PhysicsSystem::CUBE_MODEL_NAME = "CubeCollider";
const std::string PhysicsSystem::SPHERE_MODEL_NAME = "SphereCollider";
const std::string PhysicsSystem::CAPSULE_MODEL_NAME = "CapsuleCollider";
const std::string PhysicsSystem::DEFAULT_MATERIAL_NAME = "DefaultPhysicsMaterial";


PhysicsSystem::PhysicsSystem(const SystemArgs& args)
  : System(args) {
  assert(!Syx::testIslandAll() && "Physics tests failed");
}

PhysicsSystem::~PhysicsSystem() {
}

void PhysicsSystem::init() {
  mTimescale = 0;
  Syx::Interface::gDrawer = &mArgs.mSystems->getSystem<GraphicsSystem>()->getDebugDrawer();
  mSystem = std::make_unique<Syx::PhysicsSystem>();

  AssetRepo* assets = mArgs.mSystems->getSystem<AssetRepo>();
  std::unique_ptr<PhysicsModel> mod = AssetRepo::createAsset<PhysicsModel>(AssetInfo(CUBE_MODEL_NAME));
  mod->mSyxHandle = mSystem->getCube();
  assets->addAsset(std::move(mod));

  mod = AssetRepo::createAsset<PhysicsModel>(AssetInfo(SPHERE_MODEL_NAME));
  mod->mSyxHandle = mSystem->getSphere();
  assets->addAsset(std::move(mod));

  mod = AssetRepo::createAsset<PhysicsModel>(AssetInfo(CAPSULE_MODEL_NAME));
  mod->mSyxHandle = mSystem->getCapsule();
  assets->addAsset(std::move(mod));

  //Not actually a model, but all that's needed is a handle
  mod = AssetRepo::createAsset<PhysicsModel>(AssetInfo(DEFAULT_MATERIAL_NAME));
  mod->mSyxHandle = mSystem->getDefaultMaterial();
  assets->addAsset(std::move(mod));

  mDefaultSpace = mSystem->addSpace();

  mTransformUpdates = std::make_unique<EventBuffer>();
  mEventHandler = std::make_unique<EventHandler>();

  SYSTEM_EVENT_HANDLER(TransformEvent, _transformEvent);
  SYSTEM_EVENT_HANDLER(PhysicsCompUpdateEvent, _compUpdateEvent);
  SYSTEM_EVENT_HANDLER(SetComponentPropsEvent, _setComponentPropsEvent);
  SYSTEM_EVENT_HANDLER(ClearSpaceEvent, _clearSpaceEvent);
  SYSTEM_EVENT_HANDLER(RemoveComponentEvent, _removeComponentEvent);
  SYSTEM_EVENT_HANDLER(SetTimescaleEvent, _setTimescaleEvent);
}

void PhysicsSystem::queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  auto game = std::make_shared<FunctionTask>([this]() {
    mEventHandler->handleEvents(*mEventBuffer);
  });

  auto update = std::make_shared<FunctionTask>([this, dt]() {
    mSystem->update(dt*mTimescale);
  });

  auto events = std::make_shared<FunctionTask>([this]() {
    _processSyxEvents();
  });

  game->then(update)->then(events)->then(frameTask);

  pool.queueTask(game);
  pool.queueTask(update);
  pool.queueTask(events);
}

void PhysicsSystem::_processSyxEvents() {
  mTransformUpdates->clear();
  const Syx::EventListener<Syx::UpdateEvent>* updates = mSystem->getUpdateEvents(mDefaultSpace);
  if(updates) {
    for(const Syx::UpdateEvent& e : updates->mEvents) {
      auto it = mFromSyx.find(e.mHandle);
      if(it != mFromSyx.end()) {
        const SyxData& data = mToSyx[it->second];
        _updateObject(it->second, data, e);
      }
      else
        printf("Failed to map physics object %u\n", e.mHandle);
    }
  }
  else
    printf("Failed to get physics update events\n");

  {
    MessageQueue m = mArgs.mMessages->getMessageQueue();
    mTransformUpdates->appendTo(m);
  }
  mTransformUpdates->clear();
}

void PhysicsSystem::_updateObject(Handle obj, const SyxData& data, const Syx::UpdateEvent& e) {
  Syx::Mat4 newTransform = Syx::Mat4::transform(data.mSyxToModel.getScale().reciprocal(), e.mRot, e.mPos) * data.mSyxToModel;
  mTransformUpdates->push(TransformEvent(obj, newTransform, GetSystemID(PhysicsSystem)));
}

void PhysicsSystem::_setComponentPropsEvent(const SetComponentPropsEvent& e) {
  if(e.mCompType.id == Component::typeId<Physics>()) {
    SyxData& syxData = _getSyxData(e.mObj, false, false);
    Syx::Handle h = syxData.mHandle;
    Physics comp(0);
    e.mProp->copyFromBuffer(&comp, e.mBuffer.data());
    const PhysicsData& data = comp.getData();
    e.mProp->forEachDiff(e.mDiff, &comp, [&data, &syxData, this, h](const Lua::Node& node, const void*) {
      switch(Util::constHash(node.getName().c_str())) {
        case Util::constHash("hasRigidbody"): mSystem->setHasRigidbody(data.mHasRigidbody, mDefaultSpace, h); break;
        case Util::constHash("hasCollider"): mSystem->setHasCollider(data.mHasCollider, mDefaultSpace, h); break;
        case Util::constHash("linVel"): mSystem->setVelocity(mDefaultSpace, h, data.mLinVel); break;
        case Util::constHash("angVel"): mSystem->setAngularVelocity(mDefaultSpace, h, data.mAngVel); break;
        case Util::constHash("model"): {
          if(auto modelAsset = mArgs.mSystems->getSystem<AssetRepo>()->getAsset(AssetInfo(data.mModel))) {
            if(const PhysicsModel* pmod = modelAsset->cast<PhysicsModel>()) {
              mSystem->setObjectModel(mDefaultSpace, h, pmod->getSyxHandle());
            }
          }
          break;
        }
        case Util::constHash("material"): {
          if(auto materialAsset = mArgs.mSystems->getSystem<AssetRepo>()->getAsset(AssetInfo(data.mMaterial))) {
            if(const PhysicsModel* pmat = materialAsset->cast<PhysicsModel>()) {
              mSystem->setObjectMaterial(mDefaultSpace, h, pmat->getSyxHandle());
            }
          }
          break;
        }
        case Util::constHash("physToModel"): syxData.mSyxToModel = data.mPhysToModel; break;
        default: assert(false && "Unhandled property setter"); break;
      }
    });
  }
  else if(e.mCompType.id == Component::typeId<Transform>()) {
    Transform t(0);
    e.mProp->copyFromBuffer(&t, e.mBuffer.data(), e.mDiff);
    _updateTransform(e.mObj, t.get());
  }
}

void PhysicsSystem::_clearSpaceEvent(const ClearSpaceEvent& e) {
  //TODO: use scene id
  mToSyx.clear();
  mFromSyx.clear();
  mSystem->clearSpace(mDefaultSpace);
}

void PhysicsSystem::_removeComponentEvent(const RemoveComponentEvent& e) {
  if(e.mCompType == Component::typeId<Physics>()) {
    const auto& it = mToSyx.find(e.mObj);
    if(it != mToSyx.end()) {
      mSystem->removePhysicsObject(mDefaultSpace, it->second.mHandle);
    }
  }
}

void PhysicsSystem::_setTimescaleEvent(const SetTimescaleEvent& e) {
  mTimescale = e.mTimescale;
}

void PhysicsSystem::_compUpdateEvent(const PhysicsCompUpdateEvent& e) {
  _updateFromData(e.mOwner, e.mData);
}

PhysicsSystem::SyxData& PhysicsSystem::_getSyxData(Handle obj, bool hasRigidbody, bool hasCollider) {
  auto it = mToSyx.find(obj);
  if(it != mToSyx.end())
    return it->second;
  _createObject(obj, hasRigidbody, hasCollider);
  return mToSyx.find(obj)->second;
}

void PhysicsSystem::_updateFromData(Handle obj, const PhysicsData& data) {
  SyxData& syxData = _getSyxData(obj, data.mHasRigidbody, data.mHasCollider);
  Syx::Handle h = syxData.mHandle;

  syxData.mSyxToModel = data.mPhysToModel;

  mSystem->setVelocity(mDefaultSpace, h, data.mLinVel);
  mSystem->setAngularVelocity(mDefaultSpace, h, data.mAngVel);
  mSystem->setHasCollider(data.mHasCollider, mDefaultSpace, h);
  mSystem->setHasRigidbody(data.mHasRigidbody, mDefaultSpace, h);
  mSystem->setObjectModel(mDefaultSpace, h, data.mModel);
  mSystem->setObjectMaterial(mDefaultSpace, h, data.mMaterial);
}

void PhysicsSystem::_transformEvent(const TransformEvent& e) {
  if(e.mFromSystem == GetSystemID(PhysicsSystem))
    return;
  _updateTransform(e.mHandle, e.mTransform);
}

void PhysicsSystem::_updateTransform(Handle handle, const Syx::Mat4& mat) {
  auto it = mToSyx.find(handle);
  if(it != mToSyx.end()) {
    Syx::Vec3 pos, scale;
    Syx::Mat3 rot;
    //Move the transform into syx space then decompose it
    (mat * it->second.mSyxToModel.affineInverse()).decompose(scale, rot, pos);
    Syx::Handle h = it->second.mHandle;
    mSystem->setPosition(mDefaultSpace, h, pos);
    mSystem->setRotation(mDefaultSpace, h, rot.toQuat());
    mSystem->setScale(mDefaultSpace, h, scale);
  }
}

Syx::Handle PhysicsSystem::_createObject(Handle gameobject, bool hasRigidbody, bool hasCollider) {
  Syx::Handle result = mSystem->addPhysicsObject(hasRigidbody, hasCollider, mDefaultSpace);
  SyxData d;
  d.mHandle = result;
  mToSyx[gameobject] = d;
  mFromSyx[result] = gameobject;
  return result;
}

void PhysicsSystem::uninit() {
}

Handle PhysicsSystem::addModel(const Model& model, bool environment) {
  Syx::ModelParam p;
  p.reserve(model.mVerts.size(), model.mIndices.size());

  for(const Vertex& vert : model.mVerts)
    p.addVertex(Syx::Vec3(vert.mPos[0], vert.mPos[1], vert.mPos[2]));
  for(size_t i : model.mIndices)
    p.addIndex(i);
  p.setEnvironment(environment);
  return mSystem->addModel(p);
}

void PhysicsSystem::removeModel(Handle handle) {
  mSystem->removeModel(handle);
}

Handle PhysicsSystem::addMaterial(const Syx::Material& mat) {
  return mSystem->addMaterial(mat);
}

void PhysicsSystem::removeMaterial(Handle handle) {
  mSystem->removeMaterial(handle);
}
