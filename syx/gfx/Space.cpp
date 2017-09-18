#include "Precompile.h"
#include "Space.h"
#include "Gameobject.h"

#include "component/Renderable.h"
#include "App.h"
#include "system/System.h"
#include "component/Physics.h"
#include "system/MessagingSystem.h"

Space::Space(App& app)
  : mApp(&app) {
}

Space::~Space() {
}

void Space::init() {
  using namespace Syx;

  for(Gameobject& g : mObjects.getBuffer())
    g.init();

  Gameobject* obj = createObject();
  MessagingSystem& msg = *mApp->getSystem<MessagingSystem>(SystemId::Messaging);
  std::unique_ptr<Renderable> gfx = std::make_unique<Renderable>(obj->getHandle(), msg);
  std::unique_ptr<Physics> phy;
  gfx->mModel = getApp().mAssets["bowser"];
  gfx->mDiffTex = getApp().mAssets["maze"];
  obj->addComponent(std::move(gfx));
  obj->getComponent<Transform>(ComponentType::Transform)->set(Syx::Mat4::transform(Vec3(0.1f), Quat::Identity, Vec3::Zero));
  obj->init();

  obj = createObject();
  gfx = std::make_unique<Renderable>(obj->getHandle(), msg);
  gfx->mModel = getApp().mAssets["car"];
  gfx->mDiffTex = getApp().mAssets["maze"];
  obj->addComponent(std::move(gfx));
  obj->getComponent<Transform>(ComponentType::Transform)->set(Syx::Mat4::transform(Vec3(0.5f), Quat::Identity, Vec3(8.0f, 0.0f, 0.0f)));
  obj->init();

  obj = createObject();
  gfx = std::make_unique<Renderable>(obj->getHandle(), msg);
  gfx->mModel = getApp().mAssets["cube"];
  gfx->mDiffTex = getApp().mAssets["maze"];
  obj->addComponent(std::move(gfx));
  phy = std::make_unique<Physics>(obj->getHandle(), msg);
  phy->setCollider(mApp->mAssets["pCube"], mApp->mAssets["pDefMat"]);
  phy->setPhysToModel(Syx::Mat4::scale(Syx::Vec3(2.0f)));
  obj->addComponent(std::move(phy));
  obj->getComponent<Transform>(ComponentType::Transform)->set(Syx::Mat4::transform(Vec3(10.0f, 1.0f, 10.0f), Quat::Identity, Vec3(0.0f, -10.0f, 0.0f)));
  obj->init();

  obj = createObject();
  gfx = std::make_unique<Renderable>(obj->getHandle(), msg);
  gfx->mModel = getApp().mAssets["cube"];
  gfx->mDiffTex = getApp().mAssets["maze"];
  obj->addComponent(std::move(gfx));
  phy = std::make_unique<Physics>(obj->getHandle(), msg);
  phy->setCollider(mApp->mAssets["pCube"], mApp->mAssets["pDefMat"]);
  phy->setRigidbody(Syx::Vec3::Zero, Syx::Vec3::Zero);
  phy->setPhysToModel(Syx::Mat4::scale(Syx::Vec3(2.0f)));
  phy->setAngVel(Vec3(3.0f));
  obj->addComponent(std::move(phy));
  obj->getComponent<Transform>(ComponentType::Transform)->set(Syx::Mat4::transform(Vec3(1.0f, 1.0f, 1.0f), Quat::Identity, Vec3(0.0f, 8.0f, 0.0f)));
  obj->init();
}

void Space::update(float dt) {
}

void Space::uninit() {
  for(Gameobject& g : mObjects.getBuffer())
    g.uninit();
}

Gameobject* Space::createObject() {
  Handle h = mObjectGen.next();
  return &mObjects.pushBack(Gameobject(h, mApp->getSystem<MessagingSystem>(SystemId::Messaging)), h);
}

App& Space::getApp() {
  return *mApp;
}

GuardWrapped<MappedBuffer<Gameobject>> Space::getObjects() {
  return GuardWrapped<MappedBuffer<Gameobject>>(mObjects, mObjectsMutex);
}
