#include "Precompile.h"
#include "system/PhysicsSystem.h"
#include "Model.h"
#include "system/GraphicsSystem.h"
#include "App.h"
#include "system/MessagingSystem.h"
#include "event/Event.h"
#include "component/Physics.h"

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

  mApp->mAssets["pCube"] = mSystem->GetCube();
  mApp->mAssets["pSphere"] = mSystem->GetSphere();
  mApp->mAssets["pCapsule"] = mSystem->GetCapsule();
  mApp->mAssets["pDefMat"] = mSystem->GetDefaultMaterial();

  mDefaultSpace = mSystem->AddSpace();

  mEventListener = std::make_unique<EventListener>(EventFlag::Physics);
  mTransformListener = std::make_unique<TransformListener>();
  MessagingSystem& msg = mApp->getSystem<MessagingSystem>(SystemId::Messaging);
  msg.addEventListener(*mEventListener);
  msg.addTransformListener(*mTransformListener);
}

void PhysicsSystem::update(float dt) {
  _processEvents();
  mSystem->Update(dt);
}

void PhysicsSystem::_processEvents() {
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

void PhysicsSystem::_compUpdateEvent(const PhysicsCompUpdateEvent& e) {
  auto it = mToSyx.find(e.mOwner);
  Syx::Handle h;
  if(it == mToSyx.end())
    h = _createObject(e.mOwner, e.mData.mHasRigidbody, e.mData.mHasCollider);
  else
    h = it->second;

  mSystem->SetVelocity(mDefaultSpace, h, e.mData.mLinVel);
  mSystem->SetAngularVelocity(mDefaultSpace, h, e.mData.mAngVel);
  mSystem->SetHasCollider(e.mData.mHasCollider, mDefaultSpace, h);
  mSystem->SetHasRigidbody(e.mData.mHasRigidbody, mDefaultSpace, h);
  mSystem->SetObjectModel(mDefaultSpace, h, e.mData.mModel);
  mSystem->SetObjectMaterial(mDefaultSpace, h, e.mData.mMaterial);
}

void PhysicsSystem::_transformEvent(const TransformEvent& e) {
  auto it = mToSyx.find(e.mHandle);
  if(it != mToSyx.end()) {
    Syx::Vec3 pos, scale;
    Syx::Mat3 rot;
    e.mTransform.decompose(scale, rot, pos);
    mSystem->SetPosition(mDefaultSpace, it->second, pos);
    mSystem->SetRotation(mDefaultSpace, it->second, rot.ToQuat());
    mSystem->SetScale(mDefaultSpace, it->second, scale);
  }
}

Syx::Handle PhysicsSystem::_createObject(Handle gameobject, bool hasRigidbody, bool hasCollider) {
  Syx::Handle result = mSystem->AddPhysicsObject(hasRigidbody, hasCollider, mDefaultSpace);
  mToSyx[gameobject] = result;
  mFromSyx[result] = gameobject;
  return result;
}

void PhysicsSystem::uninit() {
}

Handle PhysicsSystem::addModel(const Model& model, bool environment) {
  Syx::ModelParam p;
  p.Reserve(model.mVerts.size(), model.mIndices.size());

  for(const Vertex& vert : model.mVerts)
    p.AddVertex(Syx::Vec3(vert.mPos[0], vert.mPos[1], vert.mPos[2]));
  for(size_t i : model.mIndices)
    p.AddIndex(i);
  p.SetEnvironment(environment);
  return mSystem->AddModel(p);
}

void PhysicsSystem::removeModel(Handle handle) {
  mSystem->RemoveModel(handle);
}

Handle PhysicsSystem::addMaterial(const Syx::Material& mat) {
  return mSystem->AddMaterial(mat);
}

void PhysicsSystem::removeMaterial(Handle handle) {
  mSystem->RemoveMaterial(handle);
}
