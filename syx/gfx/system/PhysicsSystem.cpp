#include "Precompile.h"
#include "system/PhysicsSystem.h"
#include "Model.h"
#include "system/GraphicsSystem.h"
#include "App.h"

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
}

void PhysicsSystem::update(float dt) {
  mSystem->Update(dt);
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
