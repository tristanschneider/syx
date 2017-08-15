#include "Precompile.h"
#include "Space.h"
#include "Gameobject.h"

#include "components/GraphicsComponent.h"
#include "App.h"

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
  std::unique_ptr<GraphicsComponent> gfx = std::make_unique<GraphicsComponent>(obj->getHandle());
  gfx->mModel = getApp().mAssets["bowser"];
  gfx->mDiffTex = getApp().mAssets["maze"];
  obj->addComponent(std::move(gfx));
  obj->getComponent<TransformComponent>(ComponentType::Transform)->mMat = Syx::Mat4::transform(Vec3(0.1f), Quat::Identity, Vec3::Zero);

  obj = createObject();
  gfx = std::make_unique<GraphicsComponent>(obj->getHandle());
  gfx->mModel = getApp().mAssets["car"];
  gfx->mDiffTex = getApp().mAssets["maze"];
  obj->addComponent(std::move(gfx));
  obj->getComponent<TransformComponent>(ComponentType::Transform)->mMat = Syx::Mat4::transform(Vec3(0.5f), Quat::Identity, Vec3(8.0f, 0.0f, 0.0f));

}

void Space::update(float dt) {
  for(Gameobject& g : mObjects.getBuffer())
    g.update(dt);
}

void Space::uninit() {
  for(Gameobject& g : mObjects.getBuffer())
    g.uninit();
}

Gameobject* Space::createObject() {
  Handle h = mObjectGen.Next();
  return &mObjects.pushBack(Gameobject(h), h);
}

App& Space::getApp() {
  return *mApp;
}

