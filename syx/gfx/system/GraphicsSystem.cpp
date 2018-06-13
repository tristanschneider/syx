#include "Precompile.h"
#include "system/GraphicsSystem.h"

#include "asset/Model.h"
#include "asset/Shader.h"
#include "asset/Texture.h"
#include "Camera.h"
#include "component/Renderable.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"
#include "DebugDrawer.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/TransformEvent.h"
#include "event/BaseComponentEvents.h"
#include "event/DebugDrawEvent.h"
#include "event/SpaceEvents.h"
#include "lua/LuaNode.h"
#include "ImGuiImpl.h"
#include "system/KeyboardInput.h"
#include "system/AssetRepo.h"
#include "provider/SystemProvider.h"

using namespace Syx;

RegisterSystemCPP(GraphicsSystem);

GraphicsSystem::LocalRenderable::LocalRenderable(Handle h)
  : mHandle(h)
  , mSpace(0)
  , mModel(nullptr)
  , mDiffTex(nullptr)
  , mTransform(Syx::Mat4::identity()) {
}

Handle GraphicsSystem::LocalRenderable::getHandle() const {
  return mHandle;
}

GraphicsSystem::~GraphicsSystem() {
}

GraphicsSystem::GraphicsSystem(const SystemArgs& args)
  : System(args) {
}

void GraphicsSystem::init() {
  mCamera = std::make_unique<Camera>(CameraOps(1.396f, 1.396f, 0.1f, 100.0f));
  mGeometry = mArgs.mSystems->getSystem<AssetRepo>()->getAsset(AssetInfo("shaders/phong.vs"));

  Mat4 ct = mCamera->getTransform();
  ct.setTranslate(Vec3(0.0f, 0.0f, -3.0f));
  ct.setRot(Quat::lookAt(-Vec3::UnitZ));
  mCamera->setTransform(ct);
  mDebugDrawer = std::make_unique<DebugDrawer>(*mArgs.mSystems->getSystem<AssetRepo>());
  mImGui = std::make_unique<ImGuiImpl>();

  mEventHandler = std::make_unique<EventHandler>();
  SYSTEM_EVENT_HANDLER(AddComponentEvent, _processAddEvent);
  SYSTEM_EVENT_HANDLER(RemoveComponentEvent, _processRemoveEvent);
  SYSTEM_EVENT_HANDLER(RenderableUpdateEvent, _processRenderableEvent);
  SYSTEM_EVENT_HANDLER(TransformEvent, _processTransformEvent);
  SYSTEM_EVENT_HANDLER(DrawLineEvent, _processDebugDrawEvent);
  SYSTEM_EVENT_HANDLER(DrawVectorEvent, _processDebugDrawEvent);
  SYSTEM_EVENT_HANDLER(DrawPointEvent, _processDebugDrawEvent);
  SYSTEM_EVENT_HANDLER(DrawCubeEvent, _processDebugDrawEvent);
  SYSTEM_EVENT_HANDLER(DrawSphereEvent, _processDebugDrawEvent);
  SYSTEM_EVENT_HANDLER(SetComponentPropsEvent, _processSetCompPropsEvent);
  SYSTEM_EVENT_HANDLER(ClearSpaceEvent, _processClearSpaceEvent);
}

void GraphicsSystem::update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  mEventHandler->handleEvents(*mEventBuffer);
  //Can't really do anything on background threads at the moment because this one has the context.
  _render(dt);
  if(mImGui) {
    mImGui->render(dt, mScreenSize);
    mImGui->updateInput(*mArgs.mSystems->getSystem<KeyboardInput>());
  }
  _processRenderThreadTasks();
}

void GraphicsSystem::uninit() {
}

Camera& GraphicsSystem::getPrimaryCamera() {
  return *mCamera;
}

DebugDrawer& GraphicsSystem::getDebugDrawer() {
  return *mDebugDrawer;
}

template<typename Resource>
void removeResource(Handle handle, std::unordered_map<Handle, Resource>& resMap, const char* errorMsg) {
  auto it = resMap.find(handle);
  if(it == resMap.end()) {
    printf(errorMsg);
    return;
  }
  it->second.unloadGpu();
  resMap.erase(it);
}

void GraphicsSystem::dispatchToRenderThread(std::function<void()> func) {
  mTasksMutex.lock();
  mTasks.push_back(func);
  mTasksMutex.unlock();
}

void GraphicsSystem::_processAddEvent(const AddComponentEvent& e) {
  if(e.mCompType == Component::typeId<Renderable>())
    mLocalRenderables.pushBack(LocalRenderable(e.mObj));
}

void GraphicsSystem::_processRemoveEvent(const RemoveComponentEvent& e) {
  if(e.mCompType == Component::typeId<Renderable>())
    mLocalRenderables.erase(e.mObj);
}

void GraphicsSystem::_processTransformEvent(const TransformEvent& e) {
  LocalRenderable* obj = mLocalRenderables.get(e.mHandle);
  if(obj) {
    obj->mTransform = e.mTransform;
  }
}

void GraphicsSystem::_processRenderableEvent(const RenderableUpdateEvent& e) {
  LocalRenderable* obj = mLocalRenderables.get(e.mObj);
  if(obj) {
    _setFromData(*obj, e.mData);
  }
}

void GraphicsSystem::_processDebugDrawEvent(const DrawLineEvent& e) {
  mDebugDrawer->drawLine(e.mStart, e.mEnd, e.mColor);
}

void GraphicsSystem::_processDebugDrawEvent(const DrawVectorEvent& e) {
  mDebugDrawer->setColor(e.mColor);
  mDebugDrawer->drawVector(e.mStart, e.mDir);
}

void GraphicsSystem::_processDebugDrawEvent(const DrawPointEvent& e) {
  mDebugDrawer->setColor(e.mColor);
  mDebugDrawer->DrawPoint(e.mPoint, e.mSize);
}

void GraphicsSystem::_processDebugDrawEvent(const DrawCubeEvent& e) {
  mDebugDrawer->setColor(e.mColor);
  mDebugDrawer->DrawCube(e.mCenter, e.mSize, e.mRot.getRight(), e.mRot.getUp());
}

void GraphicsSystem::_processDebugDrawEvent(const DrawSphereEvent& e) {
  mDebugDrawer->setColor(e.mColor);
  mDebugDrawer->DrawSphere(e.mCenter, e.mRadius, e.mRot.getRight(), e.mRot.getUp());
}

void GraphicsSystem::_processSetCompPropsEvent(const SetComponentPropsEvent& e) {
  if(e.mCompType == Component::typeId<Renderable>()) {
    if(LocalRenderable* obj = mLocalRenderables.get(e.mObj)) {
      //Make a local renderable that has the new properties
      Renderable renderable(0);
      e.mProp->copyFromBuffer(&renderable, e.mBuffer.data());
      AssetRepo& repo = *mArgs.mSystems->getSystem<AssetRepo>();
      //Assign the properties that changed, pulling the desired asset given the handle
      e.mProp->forEachDiff(e.mDiff, &renderable, [&obj, &renderable, &repo](const Lua::Node& node, const void*) {
        switch(Util::constHash(node.getName().c_str())) {
          case Util::constHash("model"): obj->mModel = repo.getAsset(AssetInfo(renderable.get().mModel)); break;
          case Util::constHash("diffuseTexture"): obj->mDiffTex = repo.getAsset(AssetInfo(renderable.get().mDiffTex)); break;
        }
      });
    }
  }
  else if(e.mCompType == Component::typeId<Transform>()) {
    LocalRenderable* obj = mLocalRenderables.get(e.mObj);
    if(obj) {
      Transform t(0);
      t.getLuaProps()->copyConstructFromBuffer(&t, e.mBuffer.data());
      obj->mTransform = t.get();
    }
  }
  else if(e.mCompType == Component::typeId<SpaceComponent>()) {
    if(LocalRenderable* obj = mLocalRenderables.get(e.mObj)) {
      SpaceComponent s(0);
      s.getLuaProps()->copyConstructFromBuffer(&s, e.mBuffer.data());
      obj->mSpace = s.get();
    }
  }
}

void GraphicsSystem::_processClearSpaceEvent(const ClearSpaceEvent& e) {
  std::vector<Handle> removed;
  for(const auto& renderable : mLocalRenderables.getBuffer())
    if(renderable.mSpace == e.mSpace)
      removed.push_back(renderable.mHandle);

  for(Handle h : removed)
    mLocalRenderables.erase(h);
}

void GraphicsSystem::_processRenderThreadTasks() {
  mTasksMutex.lock();
  mTasks.swap(mLocalTasks);
  mTasksMutex.unlock();

  for(auto& task : mLocalTasks)
    task();
  mLocalTasks.clear();
}

void GraphicsSystem::_setFromData(LocalRenderable& renderable, const RenderableData& data) {
  AssetRepo& repo = *mArgs.mSystems->getSystem<AssetRepo>();
  renderable.mModel = repo.getAsset(AssetInfo(data.mModel));
  renderable.mDiffTex = repo.getAsset(AssetInfo(data.mDiffTex));
}

void GraphicsSystem::_render(float dt) {
  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  mDebugDrawer->_render(mCamera->getWorldToView());

  if(mGeometry->getState() != AssetState::PostProcessed)
    return;

  {
    Texture emptyTexture(AssetInfo(0));
    Shader& geometry = static_cast<Shader&>(*mGeometry);
    Shader::Binder sb(geometry);
    Vec3 camPos = mCamera->getTransform().getTranslate();
    Vec3 mDiff(1.0f);
    Vec3 mSpec(0.6f, 0.6f, 0.6f, 2.5f);
    Vec3 mAmb(0.22f, 0.22f, 0.22f);
    Vec3 sunDir = -Vec3::Identity.normalized();
    Vec3 sunColor = Vec3::Identity;
    Mat4 wvp = mCamera->getWorldToView();

    {
      Vec3 p(3.0f);
      mDebugDrawer->drawLine(p, p + sunDir, sunColor);
      mDebugDrawer->drawLine(p + sunDir, p + sunDir - Vec3(0.1f));
    }

    glUniform3f(geometry.getUniform("uCamPos"), camPos.x, camPos.y, camPos.z);
    glUniform3f(geometry.getUniform("uDiffuse"), mDiff.x, mDiff.y, mDiff.z);
    glUniform3f(geometry.getUniform("uAmbient"), mAmb.x, mAmb.y, mAmb.z);
    glUniform4f(geometry.getUniform("uSpecular"), mSpec.x, mSpec.y, mSpec.z, mSpec.w);
    glUniform3f(geometry.getUniform("uSunDir"), sunDir.x, sunDir.y, sunDir.z);
    glUniform3f(geometry.getUniform("uSunColor"), sunColor.x, sunColor.y, sunColor.z);

    for(LocalRenderable& obj : mLocalRenderables.getBuffer()) {
      //TODO: skip objects not visible to the spaces the camera can see
      if(!obj.mModel || obj.mModel->getState() != AssetState::PostProcessed)
        continue;

      Mat4 mw = obj.mTransform;
      Mat4 mvp = wvp * mw;
      Vec3 camPos = mCamera->getTransform().getTranslate();

      {
        Texture::Binder tb(obj.mDiffTex && obj.mDiffTex->getState() == AssetState::PostProcessed ? static_cast<Texture&>(*obj.mDiffTex) : emptyTexture, 0);
        //Tell the sampler uniform to use the given texture slot
        glUniform1i(geometry.getUniform("uTex"), 0);
        {
          Model& model = static_cast<Model&>(*obj.mModel);
          Model::Binder mb(model);

          glUniformMatrix4fv(geometry.getUniform("uMVP"), 1, GL_FALSE, mvp.mData);
          glUniformMatrix4fv(geometry.getUniform("uMW"), 1, GL_FALSE, mw.mData);
          model.draw();
        }
      }
    }
  }
}

void GraphicsSystem::onResize(int width, int height) {
  glViewport(0, 0, width, height);
  mScreenSize = Syx::Vec2(static_cast<float>(width), static_cast<float>(height));
}
