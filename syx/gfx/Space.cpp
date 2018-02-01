#include "Precompile.h"
#include "Space.h"
#include "Gameobject.h"

#include "component/Renderable.h"
#include "App.h"
#include "system/System.h"
#include "component/Physics.h"
#include "system/AssetRepo.h"
#include "asset/Asset.h"

Space::Space(App& app)
  : mApp(&app) {
}

Space::~Space() {
}

void Space::init() {
  using namespace Syx;

  Gameobject* obj = createObject();
  MessageQueueProvider& msg = *mApp;
  std::unique_ptr<Renderable> gfx = std::make_unique<Renderable>(obj->getHandle(), msg);
  std::unique_ptr<Physics> phy;
  RenderableData d;
  AssetRepo* repo = getApp().getSystem<AssetRepo>();
  size_t mazeTexId = repo->getAsset(AssetInfo("textures/test.bmp"))->getInfo().mId;
  d.mModel = repo->getAsset(AssetInfo("models/bowserlow.obj"))->getInfo().mId;
  d.mDiffTex = mazeTexId;
  gfx->set(d);
  obj->addComponent(std::move(gfx));
  obj->getComponent<Transform>(ComponentType::Transform)->set(Syx::Mat4::transform(Vec3(0.1f), Quat::Identity, Vec3::Zero));
  obj->init();

  obj = createObject();
  gfx = std::make_unique<Renderable>(obj->getHandle(), msg);
  d.mModel = repo->getAsset(AssetInfo("models/car.obj"))->getInfo().mId;
  d.mDiffTex = mazeTexId;
  gfx->set(d);
  obj->addComponent(std::move(gfx));
  obj->getComponent<Transform>(ComponentType::Transform)->set(Syx::Mat4::transform(Vec3(0.5f), Quat::Identity, Vec3(8.0f, 0.0f, 0.0f)));
  obj->init();

  obj = createObject();
  gfx = std::make_unique<Renderable>(obj->getHandle(), msg);
  size_t cubeModelId = repo->getAsset(AssetInfo("models/cube.obj"))->getInfo().mId;
  d.mModel = cubeModelId;
  d.mDiffTex = mazeTexId;
  gfx->set(d);
  obj->addComponent(std::move(gfx));
  phy = std::make_unique<Physics>(obj->getHandle(), msg);
  phy->setCollider(mApp->mAssets["pCube"], mApp->mAssets["pDefMat"]);
  phy->setPhysToModel(Syx::Mat4::scale(Syx::Vec3(2.0f)));
  obj->addComponent(std::move(phy));
  obj->getComponent<Transform>(ComponentType::Transform)->set(Syx::Mat4::transform(Vec3(10.0f, 1.0f, 10.0f), Quat::Identity, Vec3(0.0f, -10.0f, 0.0f)));
  obj->init();

  obj = createObject();
  gfx = std::make_unique<Renderable>(obj->getHandle(), msg);
  d.mModel = cubeModelId;
  d.mDiffTex = mazeTexId;
  gfx->set(d);
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
}

Gameobject* Space::createObject() {
  Handle h = mObjectGen.next();
  auto resultPair = mObjects.emplace(std::piecewise_construct, std::forward_as_tuple(h), std::forward_as_tuple(h, mApp));
  return &resultPair.first->second;
}

App& Space::getApp() {
  return *mApp;
}

Guarded<HandleMap<Gameobject>> Space::getObjects() {
  return Guarded<HandleMap<Gameobject>>(mObjects, mObjectsMutex);
}
