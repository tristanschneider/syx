#include "Precompile.h"
#include "system/PhysicsSystem.h"

#include "asset/Model.h"
#include "system/GraphicsSystem.h"
#include "App.h"
#include "event/Event.h"
#include "component/Physics.h"
#include "Space.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"
#include "event/TransformEvent.h"

#include <SyxIntrusive.h>
#include <SyxHandles.h>
#include <SyxHandleMap.h>
#include <SyxPhysicsSystem.h>
#include <SyxModelParam.h>

namespace Syx {
  namespace Interface {
    extern ::DebugDrawer* gDrawer;
  }
}

RegisterSystemCPP(PhysicsSystem);

PhysicsSystem::PhysicsSystem(App& app)
  : System(app) {
}

PhysicsSystem::~PhysicsSystem() {
}

void PhysicsSystem::init() {
  Syx::Interface::gDrawer = &mApp.getSystem<GraphicsSystem>()->getDebugDrawer();
  mSystem = std::make_unique<Syx::PhysicsSystem>();

  mApp.mAssets["pCube"] = mSystem->getCube();
  mApp.mAssets["pSphere"] = mSystem->getSphere();
  mApp.mAssets["pCapsule"] = mSystem->getCapsule();
  mApp.mAssets["pDefMat"] = mSystem->getDefaultMaterial();

  mDefaultSpace = mSystem->addSpace();

  mTransformUpdates = std::make_unique<EventListener>();
  mListener = std::make_unique<EventListener>();

  SYSTEM_EVENT_HANDLER(TransformEvent, _transformEvent);
  SYSTEM_EVENT_HANDLER(PhysicsCompUpdateEvent, _compUpdateEvent);
}

void PhysicsSystem::update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  auto game = std::make_shared<FunctionTask>([this]() {
    mListener->handleEvents();
  });

  auto update = std::make_shared<FunctionTask>([this, dt]() {
    mSystem->update(dt);
  });
  update->addDependency(game);

  auto events = std::make_shared<FunctionTask>([this]() {
    _processSyxEvents();
  });
  events->addDependency(update);

  //Frame isn't done until all physics events are, which ends with events task
  frameTask->addDependency(events);
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
    MessageQueue m = mApp.getMessageQueue();
    mTransformUpdates->appendTo(m);
  }
  mTransformUpdates->clear();
}

void PhysicsSystem::_updateObject(Handle obj, const SyxData& data, const Syx::UpdateEvent& e) {
  Syx::Mat4 newTransform = Syx::Mat4::transform(data.mSyxToModel.getScale().reciprocal(), e.mRot, e.mPos) * data.mSyxToModel;
  mTransformUpdates->push(TransformEvent(obj, newTransform, GetSystemID(PhysicsSystem)));
}

void PhysicsSystem::_compUpdateEvent(const PhysicsCompUpdateEvent& e) {
  auto it = mToSyx.find(e.mOwner);
  Syx::Handle h;
  if(it == mToSyx.end()) {
    h = _createObject(e.mOwner, e.mData.mHasRigidbody, e.mData.mHasCollider);
    it = mToSyx.find(e.mOwner);
  }
  else
    h = it->second.mHandle;

  it->second.mSyxToModel = e.mData.mPhysToModel;

  mSystem->setVelocity(mDefaultSpace, h, e.mData.mLinVel);
  mSystem->setAngularVelocity(mDefaultSpace, h, e.mData.mAngVel);
  mSystem->setHasCollider(e.mData.mHasCollider, mDefaultSpace, h);
  mSystem->setHasRigidbody(e.mData.mHasRigidbody, mDefaultSpace, h);
  mSystem->setObjectModel(mDefaultSpace, h, e.mData.mModel);
  mSystem->setObjectMaterial(mDefaultSpace, h, e.mData.mMaterial);
}

void PhysicsSystem::_transformEvent(const TransformEvent& e) {
  if(e.mFromSystem == GetSystemID(PhysicsSystem))
    return;
  auto it = mToSyx.find(e.mHandle);
  if(it != mToSyx.end()) {
    Syx::Vec3 pos, scale;
    Syx::Mat3 rot;
    //Move the transform into syx space then decompose it
    (e.mTransform * it->second.mSyxToModel.affineInverse()).decompose(scale, rot, pos);
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
