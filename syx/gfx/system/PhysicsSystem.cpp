#include "Precompile.h"
#include "system/PhysicsSystem.h"
#include "Model.h"
#include "system/GraphicsSystem.h"
#include "App.h"
#include "system/MessagingSystem.h"
#include "event/Event.h"
#include "component/Physics.h"
#include "Space.h"

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

PhysicsSystem::PhysicsSystem() {
}

PhysicsSystem::~PhysicsSystem() {
}

void PhysicsSystem::init() {
  Syx::Interface::gDrawer = &mApp->getSystem<GraphicsSystem>(SystemId::Graphics).getDebugDrawer();
  mSystem = std::make_unique<Syx::PhysicsSystem>();

  mApp->mAssets["pCube"] = mSystem->getCube();
  mApp->mAssets["pSphere"] = mSystem->getSphere();
  mApp->mAssets["pCapsule"] = mSystem->getCapsule();
  mApp->mAssets["pDefMat"] = mSystem->getDefaultMaterial();

  mDefaultSpace = mSystem->addSpace();

  mEventListener = std::make_unique<EventListener>(EventFlag::Physics);
  mTransformListener = std::make_unique<TransformListener>();
  mTransformUpdates = std::make_unique<std::vector<TransformEvent>>();
  MessagingSystem& msg = mApp->getSystem<MessagingSystem>(SystemId::Messaging);
  msg.addEventListener(*mEventListener);
  msg.addTransformListener(*mTransformListener);
}

void PhysicsSystem::update(float dt) {
  _processGameEvents();
  mSystem->update(dt);
  _processSyxEvents();
}

void PhysicsSystem::_processGameEvents() {
  for(const std::unique_ptr<Event>& e : mEventListener->mEvents) {
    switch(static_cast<EventType>(e->getHandle())) {
      case EventType::PhysicsCompUpdate: _compUpdateEvent(static_cast<const PhysicsCompUpdateEvent&>(*e)); break;
      default: continue;
    }
  }
  mEventListener->mEvents.clear();

  for(const TransformEvent& t : mTransformListener->mEvents)
    _transformEvent(t);
  mTransformListener->mEvents.clear();
}

void PhysicsSystem::_processSyxEvents() {
  mTransformUpdates->clear();
  const Syx::EventListener<Syx::UpdateEvent>* updates = mSystem->getUpdateEvents(mDefaultSpace);
  if(updates) {
    Space& space = mApp->getDefaultSpace();
    for(const Syx::UpdateEvent& e : updates->mEvents) {
      auto it = mFromSyx.find(e.mHandle);
      if(it != mFromSyx.end()) {
        if(Gameobject* obj = space.mObjects.get(it->second)) {
          const SyxData& data = mToSyx[it->second];
          _updateObject(*obj, data, e);
        }
        else
          printf("Failed to get game object for physics update %u\n", it->second);
      }
      else
        printf("Failed to map physics object %u\n", e.mHandle);
    }
  }
  else
    printf("Failed to get physics update events\n");

  mApp->getSystem<MessagingSystem>(SystemId::Messaging).fireTransformEvents(*mTransformUpdates, mTransformListener.get());
}

void PhysicsSystem::_updateObject(Gameobject& obj, const SyxData& data, const Syx::UpdateEvent& e) {
  Transform& t = *obj.getComponent<Transform>(ComponentType::Transform);
  Syx::Vec3 scale = t.get().getScale();
  t.set(Syx::Mat4::transform(data.mSyxToModel.getScale().reciprocal(), e.mRot, e.mPos) * data.mSyxToModel, false);
  mTransformUpdates->emplace_back(obj.getHandle(), t.get());
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
