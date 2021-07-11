#include "Precompile.h"
#include "system/PhysicsSystem.h"

#include "asset/Model.h"
#include "asset/PhysicsModel.h"
#include "component/ComponentPublisher.h"
#include "editor/event/EditorEvents.h"
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
#include <SyxIPhysicsObject.h>
#include <SyxPhysicsSystem.h>
#include <SyxMaterialRepository.h>
#include <SyxModelParam.h>
#include <SyxModel.h>
#include <SyxSpace.h>
#include "Util.h"

#include <SyxIslandTests.h>

namespace Syx {
  namespace Interface {
    extern ::IDebugDrawer* gDrawer;
    extern SyxOptions gOptions;
  }
}

const std::string PhysicsSystem::CUBE_MODEL_NAME = "CubeCollider";
const std::string PhysicsSystem::SPHERE_MODEL_NAME = "SphereCollider";
const std::string PhysicsSystem::CAPSULE_MODEL_NAME = "CapsuleCollider";
const std::string PhysicsSystem::DEFAULT_MATERIAL_NAME = "DefaultPhysicsMaterial";

PhysicsSystem::PhysicsSystem(const SystemArgs& args)
  : System(args, _typeId<PhysicsSystem>()) {
  //TODO: move to test framework
  assert(!Syx::testIslandAll() && "Physics tests failed");
}

PhysicsSystem::~PhysicsSystem() = default;

void PhysicsSystem::init() {
  mTimescale = 0;
  if(GraphicsSystem* graphics = mArgs.mSystems->getSystem<GraphicsSystem>()) {
    Syx::Interface::gDrawer = &graphics->getDebugDrawer();
  }
  mSystem = std::make_unique<Syx::PhysicsSystem>();

  mDefaultMaterial = mSystem->getMaterialRepository().addMaterial({});

  AssetRepo* assets = mArgs.mSystems->getSystem<AssetRepo>();
  std::unique_ptr<PhysicsModel> mod = AssetRepo::createAsset<PhysicsModel>(AssetInfo(CUBE_MODEL_NAME));

  mod->mSyxHandle = mDefaultModel = std::make_shared<Syx::Model>(Syx::ModelType::Cube);
  assets->addAsset(std::move(mod));

  mod = AssetRepo::createAsset<PhysicsModel>(AssetInfo(SPHERE_MODEL_NAME));
  mod->mSyxHandle = std::make_shared<Syx::Model>(Syx::ModelType::Sphere);
  assets->addAsset(std::move(mod));

  mod = AssetRepo::createAsset<PhysicsModel>(AssetInfo(CAPSULE_MODEL_NAME));
  mod->mSyxHandle = std::make_shared<Syx::Model>(Syx::ModelType::Capsule);
  assets->addAsset(std::move(mod));

  //Not actually a model, but all that's needed is a handle
  auto mat = AssetRepo::createAsset<PhysicsMaterial>(AssetInfo(DEFAULT_MATERIAL_NAME));
  mat->mSyxHandle = mDefaultMaterial;
  assets->addAsset(std::move(mat));

  mDefaultSpace = mSystem->createSpace();

  mTransformUpdates = std::make_unique<EventBuffer>();
  mEventHandler = std::make_unique<EventHandler>();

  _registerSystemEventHandler(&PhysicsSystem::_transformEvent);
  _registerSystemEventHandler(&PhysicsSystem::_setComponentPropsEvent);
  _registerSystemEventHandler(&PhysicsSystem::_clearSpaceEvent);
  _registerSystemEventHandler(&PhysicsSystem::_removeComponentEvent);
  _registerSystemEventHandler(&PhysicsSystem::_setTimescaleEvent);
  mEventHandler->registerEventHandler(CallbackEvent::getHandler(typeId<PhysicsSystem>()));
  mEventHandler->registerEventHandler([](const SetPlayStateEvent& e) {
    if(e.mState == PlayState::Playing) {
      Syx::Interface::gOptions.mDebugFlags = 0;
    }
    else {
      Syx::Interface::gOptions.mDebugFlags = Syx::SyxOptions::Debug::DrawModels
        | Syx::SyxOptions::Debug::DisableCollision;
    }
  });
  _registerSystemEventHandler(&PhysicsSystem::_onApplyForce);
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
  const Syx::EventListener<Syx::UpdateEvent>& updates = mDefaultSpace->getUpdateEvents();
  static const Lua::Node* props = Physics::singleton().getLuaProps();
  //Precompute diff ids for velocity fields
  static const Lua::NodeDiff velocityDiff = props->getChild("linVel")->_getDiffId() | props->getChild("angVel")->_getDiffId();

  for(const Syx::UpdateEvent& e : updates.mEvents) {
    auto it = mFromSyx.find(e.mHandle);
    if(it != mFromSyx.end()) {
      const SyxData& data = mToSyx.at(it->second);
      _updateObject(it->second, data, e);

      //Send updates about only the velocity fields
      //TODO: only do this if it actually changed
      if(auto rigidbody = data.mObj->tryGetRigidbody()) {
        Physics component(it->second);
        ::PhysicsData componentData;
        componentData.mLinVel = e.mLinVel;
        componentData.mAngVel = e.mAngVel;
        component.setData(componentData);
        ComponentPublisher::forcePublish(component, *mTransformUpdates, velocityDiff, typeId<PhysicsSystem>());
      }
    }
    else
      printf("Failed to map physics object %zu\n", e.mHandle);
  }

  {
    MessageQueue m = mArgs.mMessages->getMessageQueue();
    mTransformUpdates->appendTo(m);
  }
  mTransformUpdates->clear();
}

void PhysicsSystem::_updateObject(Handle obj, const SyxData& data, const Syx::UpdateEvent& e) {
  Syx::Mat4 newTransform = Syx::Mat4::transform(data.mSyxToModel.getScale().reciprocal(), e.mRot, e.mPos) * data.mSyxToModel;
  mTransformUpdates->push(TransformEvent(obj, newTransform, typeId<PhysicsSystem>()));
}

void PhysicsSystem::_setComponentPropsEvent(const SetComponentPropsEvent& e) {
  if(e.mCompType.id == Component::typeId<Physics>() && e.mFromSystem != typeId<PhysicsSystem>()) {
    SyxData& syxData = _getSyxData(e.mObj, false, false);
    Syx::Handle h = syxData.mHandle;
    Physics comp(0);
    e.mProp->copyFromBuffer(&comp, e.mBuffer.data());
    const PhysicsData& data = comp.getData();
    e.mProp->forEachDiff(e.mDiff, &comp, [&data, &syxData, this, h](const Lua::Node& node, const void*) {
      auto setRotationLocked = [&syxData](Syx::Axis axis, bool locked) {
        if(auto rigidbody = syxData.mObj->tryGetRigidbody()) {
          rigidbody->setAxisRotationLocked(axis, locked);
        }
      };

      switch(Util::constHash(node.getName().c_str())) {
        case Util::constHash("hasRigidbody"): mSystem->setHasRigidbody(data.mHasRigidbody, mDefaultSpace->_getHandle(), h); break;
        case Util::constHash("hasCollider"): mSystem->setHasCollider(data.mHasCollider, mDefaultSpace->_getHandle(), h); break;
        case Util::constHash("linVel"): mSystem->setVelocity(mDefaultSpace->_getHandle(), h, data.mLinVel); break;
        case Util::constHash("angVel"): mSystem->setAngularVelocity(mDefaultSpace->_getHandle(), h, data.mAngVel); break;
        case Util::constHash("model"): {
          if(auto modelAsset = mArgs.mSystems->getSystem<AssetRepo>()->getAsset(AssetInfo(data.mModel))) {
            if(const PhysicsModel* pmod = modelAsset->cast<PhysicsModel>()) {
              if(Syx::ICollider* collider = syxData.mObj->tryGetCollider()) {
                collider->setModel(pmod->getSyxHandle());
              }
            }
          }
          break;
        }
        case Util::constHash("material"): {
          if(const auto materialAsset = mArgs.mSystems->getSystem<AssetRepo>()->getAsset(AssetInfo(data.mMaterial))) {
            if(const auto* pmat = materialAsset->cast<PhysicsMaterial>()) {
              if(Syx::ICollider* collider = syxData.mObj->tryGetCollider()) {
                collider->setMaterial(pmat->getSyxHandle());
              }
            }
          }
          break;
        }
        case Util::constHash("physToModel"): syxData.mSyxToModel = data.mPhysToModel; break;
        case Util::constHash("lockXRotation"): setRotationLocked(Syx::Axis::X, data.mLockXRotation); break;
        case Util::constHash("lockYRotation"): setRotationLocked(Syx::Axis::Y, data.mLockYRotation); break;
        case Util::constHash("lockZRotation"): setRotationLocked(Syx::Axis::Z, data.mLockZRotation); break;
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

void PhysicsSystem::_clearSpaceEvent(const ClearSpaceEvent&) {
  //TODO: use scene id
  mToSyx.clear();
  mFromSyx.clear();
  mDefaultSpace->clear();
}

void PhysicsSystem::_removeComponentEvent(const RemoveComponentEvent& e) {
  if(e.mCompType == Component::typeId<Physics>()) {
    const auto& it = mToSyx.find(e.mObj);
    if(it != mToSyx.end()) {
      mFromSyx.erase(it->second.mHandle);
      mToSyx.erase(it);
    }
  }
}

void PhysicsSystem::_setTimescaleEvent(const SetTimescaleEvent& e) {
  mTimescale = e.mTimescale;
}

void PhysicsSystem::_onApplyForce(const ApplyForceEvent& e) {
  if(Syx::IPhysicsObject* obj = _tryGetValidPhysicsObject(e.mObj)) {
    if(Syx::IRigidbody* rigidbody = obj->tryGetRigidbody()) {
      const float dt = e.mMode == ApplyForceEvent::Mode::Force ? _getExpectedDeltaTime() : 1.f;

      if(auto force = std::get_if<ApplyForceEvent::Force>(&e.mForce)) {
        rigidbody->applyImpulse(force->mLinear * dt, force->mAngular * dt);
      }
      else if(auto forceAtPoint = std::get_if<ApplyForceEvent::ForceAtPoint>(&e.mForce)) {
        rigidbody->applyImpulseAtPoint(forceAtPoint->mForce * dt, forceAtPoint->mPoint);
      }
    }
  }
}

PhysicsSystem::SyxData& PhysicsSystem::_getSyxData(Handle obj, bool hasRigidbody, bool hasCollider) {
  auto it = mToSyx.find(obj);
  if(it != mToSyx.end())
    return it->second;
  _createObject(obj, hasRigidbody, hasCollider);
  return mToSyx.find(obj)->second;
}

PhysicsSystem::SyxData* PhysicsSystem::_tryGetSyxData(Handle gameHandle) {
  auto it = mToSyx.find(gameHandle);
  return it != mToSyx.end() ? &it->second : nullptr;
}

Syx::IPhysicsObject* PhysicsSystem::_tryGetValidPhysicsObject(Handle gameHandle) {
  auto data = _tryGetSyxData(gameHandle);
  return data && data->mObj && data->mObj->isValid() ? data->mObj.get() : nullptr;
}

float PhysicsSystem::_getExpectedDeltaTime() const {
  //TODO: not sure if this is accurate but it's good enough for now
  return 1.f/60.f * mTimescale;
}

void PhysicsSystem::_transformEvent(const TransformEvent& e) {
  if(e.mFromSystem == typeId<PhysicsSystem>())
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
    mSystem->setPosition(mDefaultSpace->_getHandle(), h, pos);
    mSystem->setRotation(mDefaultSpace->_getHandle(), h, rot.toQuat());
    mSystem->setScale(mDefaultSpace->_getHandle(), h, scale);
  }
}

Syx::Handle PhysicsSystem::_createObject(Handle gameobject, bool hasRigidbody, bool hasCollider) {
  SyxData d;
  d.mObj = mDefaultSpace->addPhysicsObject(hasRigidbody, hasCollider, *mDefaultMaterial, mDefaultModel);
  d.mHandle = d.mObj->getHandle();
  mToSyx[gameobject] = d;
  mFromSyx[d.mHandle] = gameobject;

  //Now that we care about this object, request transform data for it
  ComponentDataRequest req(gameobject, Transform::singleton().getFullType());
  req.then(typeId<PhysicsSystem>(), [gameobject, this](const ComponentDataResponse& res) {
    if(!res.mBuffer.empty()) {
      Transform t(gameobject);
      if(const Lua::Node* props = t.getLuaProps()) {
        props->copyFromBuffer(&t, res.mBuffer.data());
        _updateTransform(gameobject, t.get());
      }
    }
  });
  getMessageQueueProvider().getMessageQueue()->push(std::move(req));

  return d.mHandle;
}

void PhysicsSystem::uninit() {
}
