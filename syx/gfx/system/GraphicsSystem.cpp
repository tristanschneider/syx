#include "Precompile.h"
#include "system/GraphicsSystem.h"

#include "asset/Model.h"
#include "asset/Shader.h"
#include "asset/Texture.h"
#include "Camera.h"
#include "component/CameraComponent.h"
#include "component/Renderable.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"
#include "DebugDrawer.h"
#include "editor/event/EditorEvents.h"
#include "event/EventBuffer.h"
#include "event/EventHandler.h"
#include "event/TransformEvent.h"
#include "event/BaseComponentEvents.h"
#include "event/DebugDrawEvent.h"
#include "event/SpaceEvents.h"
#include "event/ViewportEvents.h"
#include "provider/MessageQueueProvider.h"
#include "provider/SystemProvider.h"
#include <gl/glew.h>
#include "graphics/FullScreenQuad.h"
#include "graphics/FrameBuffer.h"
#include "graphics/PixelBuffer.h"
#include "graphics/RenderCommand.h"
#include "graphics/Viewport.h"
#include "ImGuiImpl.h"
#include "lua/LuaNode.h"
#include "system/KeyboardInput.h"
#include "system/AssetRepo.h"

using namespace Syx;

RegisterSystemCPP(GraphicsSystem);

namespace {
  Vec3 encodeHandle(Handle handle) {
    assert(static_cast<Handle>(static_cast<uint32_t>(handle)) == handle && "Pick needs to be updated to work with handle values over 24 bits");
    const uint8_t fullByte = ~0;
    const Handle low = fullByte;
    const Handle med = fullByte << 8;
    const Handle high = fullByte << 16;
    return Vec3(static_cast<float>(handle & low), static_cast<float>(handle & med), static_cast<float>(handle & high));
  }

  Handle decodeHandle(const uint8_t* handle) {
    return handle[0] | (handle[1] << 8) | (handle[2] << 16);
  }

  Camera createCamera(Handle owner) {
    return Camera(CameraOps(1.396f, 1.396f, 0.1f, 100.0f, owner));
  }
}

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
  mCameras.push_back(createCamera(0));
  Camera& defaultCamera = mCameras.back();
  mActiveCamera = defaultCamera.getOps().mOwner;

  mFullScreenQuad = std::make_unique<FullScreenQuad>();

  AssetRepo& assets = *mArgs.mSystems->getSystem<AssetRepo>();
  mGeometry = assets.getAsset<Shader>(AssetInfo("shaders/phong.vs"));
  mFSQShader = assets.getAsset<Shader>(AssetInfo("shaders/fullScreenQuad.vs"));
  mFlatColorShader = assets.getAsset<Shader>(AssetInfo("shaders/flatColor.vs"));

  Mat4 ct = defaultCamera.getTransform();
  ct.setTranslate(Vec3(0.0f, 0.0f, -3.0f));
  ct.setRot(Quat::lookAt(-Vec3::UnitZ));
  defaultCamera.setTransform(ct);
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
  SYSTEM_EVENT_HANDLER(ScreenPickRequest, _processScreenPickRequest);
  SYSTEM_EVENT_HANDLER(RenderCommandEvent, _processRenderCommandEvent);
  SYSTEM_EVENT_HANDLER(SetActiveCameraEvent, _processSetActiveCameraEvent);
  SYSTEM_EVENT_HANDLER(SetViewportEvent, _processSetViewportEvent);
  SYSTEM_EVENT_HANDLER(RemoveViewportEvent, _processRemoveViewportEvent);
}

void GraphicsSystem::update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  mEventHandler->handleEvents(*mEventBuffer);

  bool updatePick = mPickRequests.size() && mFrameBuffer;
  if(updatePick) {
    _drawPickScene(*mFrameBuffer);
    mPixelPackBuffer->download(*mFrameBuffer);
  }

  //Can't really do anything on background threads at the moment because this one has the context.
  for(const Camera& c : mCameras) {
    if(const Viewport* v = _getViewport(c.getOps().mViewport)) {
      _render(c, *v);
    }
  }

  if(updatePick) {
    std::vector<uint8_t> buffer;
    mPixelPackBuffer->mapBuffer(buffer);
    _processPickRequests(buffer, mPickRequests);
    mPickRequests.clear();
  }

  if(mImGui) {
    mImGui->render(dt, mScreenSize);
    mImGui->updateInput(*mArgs.mSystems->getSystem<KeyboardInput>());
  }
  _processRenderThreadTasks();
  mRenderCommands.clear();
}

void GraphicsSystem::uninit() {
}

Camera& GraphicsSystem::getPrimaryCamera() {
  return mCameras.front();
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
  if(e.mCompType == Component::typeId<Renderable>() && !mLocalRenderables.get(e.mObj))
    mLocalRenderables.pushBack(LocalRenderable(e.mObj));
  else if(e.mCompType == Component::typeId<CameraComponent>() && !_getCamera(e.mObj))
    mCameras.push_back(createCamera(e.mObj));
}

void GraphicsSystem::_processRemoveEvent(const RemoveComponentEvent& e) {
  if(e.mCompType == Component::typeId<Renderable>())
    mLocalRenderables.erase(e.mObj);
  else if(e.mCompType == Component::typeId<CameraComponent>()) {
    //TODO: remove
  }
}

void GraphicsSystem::_processTransformEvent(const TransformEvent& e) {
  LocalRenderable* obj = mLocalRenderables.get(e.mHandle);
  if(obj) {
    obj->mTransform = e.mTransform;
  }
  //TODO camera transform update
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
  if(e.mCompType.id == Component::typeId<Renderable>()) {
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
  else if(e.mCompType.id == Component::typeId<Transform>()) {
    LocalRenderable* obj = mLocalRenderables.get(e.mObj);
    if(obj) {
      Transform t(0);
      t.getLuaProps()->copyFromBuffer(&t, e.mBuffer.data());
      obj->mTransform = t.get();
    }
    if(Camera* camera = _getCamera(e.mObj)) {
      Transform t(0);
      t.getLuaProps()->copyFromBuffer(&t, e.mBuffer.data());
      camera->setTransform(t.get());
    }
  }
  else if(e.mCompType.id == Component::typeId<SpaceComponent>()) {
    if(LocalRenderable* obj = mLocalRenderables.get(e.mObj)) {
      SpaceComponent s(0);
      s.getLuaProps()->copyConstructFromBuffer(&s, e.mBuffer.data());
      obj->mSpace = s.get();
    }
  }
  else if(e.mCompType.id == Component::typeId<CameraComponent>()) {
    if(Camera* camera = _getCamera(e.mObj)) {
      CameraComponent c(0);
      c.getLuaProps()->copyFromBuffer(&c, e.mBuffer.data(), e.mDiff);
      CameraComponent(0).getLuaProps()->forEachDiff(e.mDiff, &c, [camera](const Lua::Node& node, const void* data) {
        switch(Util::constHash(node.getName().c_str())) {
          case Util::constHash("viewport"): camera->setViewport(*static_cast<const std::string*>(data));
          default: break;
        }
      });
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

void GraphicsSystem::_render(const Camera& camera, const Viewport& viewport) {
  glViewport(static_cast<int>(viewport.getMin().x*mScreenSize.x),
    static_cast<int>(viewport.getMin().y*mScreenSize.y),
    static_cast<int>((viewport.getMax().x - viewport.getMin().x)*mScreenSize.x),
    static_cast<int>((viewport.getMax().y - viewport.getMin().y)*mScreenSize.y));
  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  mDebugDrawer->_render(camera.getWorldToView());

  Shader* requiredShaders[] = {
    mGeometry.get(),
  };

  for(Shader* shader : requiredShaders) {
    if(shader->getState() != AssetState::PostProcessed)
      return;
  }

  {
    const Texture emptyTexture(AssetInfo(0));
    Shader& geometry = *mGeometry;
    const Shader::Binder sb(geometry);
    const Vec3 camPos = camera.getTransform().getTranslate();
    const Vec3 mDiff(1.0f);
    const Vec3 mSpec(0.6f, 0.6f, 0.6f, 2.5f);
    const Vec3 mAmb(0.22f, 0.22f, 0.22f);
    const Vec3 sunDir = -Vec3::Identity.normalized();
    const Vec3 sunColor = Vec3::Identity;
    const Mat4 wvp = camera.getWorldToView();

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

      const Mat4 mw = obj.mTransform;
      const Mat4 mvp = wvp * mw;

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

  _renderCommands();
}

void GraphicsSystem::_renderCommands() {
  for(const RenderCommand& c : mRenderCommands) {
    switch(c.mType) {
      case RenderCommand::Type::Outline: _outline(c); break;
      case RenderCommand::Type::Quad2d: _quad2d(c); break;
    }
  }
}

void GraphicsSystem::_outline(const RenderCommand& c) {
  if(mFlatColorShader->getState() != AssetState::PostProcessed)
    return;
  const LocalRenderable* obj = mLocalRenderables.get(c.mOutline.mHandle);
  if(!obj || !obj->mModel || obj->mModel->getState() != AssetState::PostProcessed)
    return;

  const int stencilWrite = 0;
  const int outline = 1;
  GLint prevStencilFunc;
  glGetIntegerv(GL_STENCIL_FUNC, &prevStencilFunc);
  const GLboolean prevBlend = glIsEnabled(GL_BLEND);
  const GLboolean prevStencil = glIsEnabled(GL_STENCIL_TEST);
  float prevLineWidth;
  glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);

  glEnable(GL_STENCIL_TEST);

  const Shader::Binder sb(*mFlatColorShader);
  const Model& model = static_cast<Model&>(*obj->mModel);
  const Model::Binder mb(model);

  const Mat4 wvp = _getActiveCamera().getWorldToView();
  const Mat4 mw = obj->mTransform;
  const Mat4 mvp = wvp * mw;
  glUniformMatrix4fv(mFlatColorShader->getUniform("mvp"), 1, GL_FALSE, mvp.mData);
  //Since the object has already been drawn, re draw it slightly above so the draw doesn't fail depth tests
  glUniform1f(mFlatColorShader->getUniform("depthBias"), -0.01f);
  glUniform4f(mFlatColorShader->getUniform("uColor"), c.mOutline.mColor[0], c.mOutline.mColor[1], c.mOutline.mColor[2], 0.0f);
  glEnable(GL_BLEND);

  for(int pass = 0; pass < 2; ++pass) {
    if(pass == stencilWrite) {
      //Write model's shape to stencil buffer
      glStencilFunc(GL_ALWAYS, 1, -1);
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      //Draw nothing, zero from source, one from dest, just used to write stencil
      glBlendFunc(GL_ZERO, GL_ONE);
    }
    else if(pass == outline) {
      //Draw with line primitives, only ones that lie outside the stencil.
      glLineWidth(c.mOutline.mWidth);
      glPolygonMode(GL_FRONT, GL_LINE);
      glStencilFunc(GL_NOTEQUAL, 1, -1);
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      //Set this back to default
      glDisable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ZERO);
    }

    model.draw();
  }

  //Restore old states
  glPolygonMode(GL_FRONT, GL_FILL);
  if(!prevStencil) glDisable(GL_STENCIL_TEST);
  if(prevBlend) glEnable(GL_BLEND);
  glStencilFunc(prevStencilFunc, 1, -1);
  glLineWidth(prevLineWidth);
}

void GraphicsSystem::_quad2d(const RenderCommand& c) {
  if(mFlatColorShader->getState() != AssetState::PostProcessed)
    return;

  const auto& q = c.mQuad2d;
  Vec2 min(q.mMin[0], q.mMin[1]);
  Vec2 max(q.mMax[0], q.mMax[1]);
  Mat4 mvp = Mat4::identity();

  // Flip origin from top left to bottom right
  min.y = mScreenSize.y - min.y;
  max.y = mScreenSize.y - max.y;
  if(min.y > max.y)
    std::swap(min.y, max.y);

  Vec2 origin = (min + max)*0.5f;
  Vec2 scale = (max - min)*0.5f;
  mvp = Mat4::translate(Vec3(-1, -1, 0)) * Mat4::scale(Vec3(2.0f/mScreenSize.x, 2.0f/mScreenSize.y, 0.0f)) * Mat4::translate(Vec3(origin.x, origin.y, 0.0f)) * Mat4::scale(Vec3(scale.x, scale.y, 0.0f));

  if(q.mColor[3] != 0 && q.mColor[3] != 1) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  Shader::Binder b(*mFlatColorShader);
  glUniform4f(mFlatColorShader->getUniform("uColor"), q.mColor[0], q.mColor[1], q.mColor[2], q.mColor[3]);
  glUniformMatrix4fv(mFlatColorShader->getUniform("mvp"), 1, GL_FALSE, mvp.mData);
  mFullScreenQuad->draw();

  glDisable(GL_BLEND);
}

void GraphicsSystem::_drawTexture(const Texture& tex, const Syx::Vec2& origin, const Syx::Vec2& size) {
  if(tex.getState() == AssetState::PostProcessed) {
    Texture::Binder b(tex, 0);
    _drawBoundTexture(origin, size);
  }
}

void GraphicsSystem::_drawBoundTexture(const Syx::Vec2& origin, const Syx::Vec2& size) {
  if(mFSQShader->getState() == AssetState::PostProcessed) {
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glViewport(static_cast<int>(origin.x), static_cast<int>(origin.y), static_cast<int>(size.x), static_cast<int>(size.y));
    Shader::Binder b(*mFSQShader);
    mFullScreenQuad->draw();
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  }
}

void GraphicsSystem::_drawPickScene(const FrameBuffer& destination) {
  if(mFlatColorShader->getState() != AssetState::PostProcessed) {
    return;
  }
  destination.bind();
  glViewport(0, 0, (int)mScreenSize.x, (int)mScreenSize.y);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  {
    const Shader::Binder sb(*mFlatColorShader);
    const Camera& camera = _getActiveCamera();
    const Vec3 camPos = camera.getTransform().getTranslate();
    const Mat4 wvp = camera.getWorldToView();

    for(LocalRenderable& obj : mLocalRenderables.getBuffer()) {
      //TODO: skip objects not visible to the spaces the camera can see
      if(!obj.mModel || obj.mModel->getState() != AssetState::PostProcessed)
        continue;

      const Mat4 mw = obj.mTransform;
      const Mat4 mvp = wvp * mw;
      const Model& model = static_cast<Model&>(*obj.mModel);
      const Model::Binder mb(model);
      glUniformMatrix4fv(mFlatColorShader->getUniform("mvp"), 1, GL_FALSE, mvp.mData);
      const Vec3 handleColor = encodeHandle(obj.getHandle())*(1.0f/255.0f);
      glUniform4fv(mFlatColorShader->getUniform("uColor"), 1, &handleColor.x);

      model.draw();
    }
  }
  destination.unBind();
}

void GraphicsSystem::_processScreenPickRequest(const ScreenPickRequest& e) {
  mPickRequests.push_back(e);
}

void GraphicsSystem::_processRenderCommandEvent(const RenderCommandEvent& e) {
  mRenderCommands.emplace_back(e.mCmd);
}

void GraphicsSystem::_processSetActiveCameraEvent(const SetActiveCameraEvent& e) {
  if(std::find_if(mCameras.begin(), mCameras.end(), [&e](const Camera& c) { return c.getOps().mOwner == e.mHandle; }) != mCameras.end())
    mActiveCamera = e.mHandle;
}

void GraphicsSystem::_processSetViewportEvent(const SetViewportEvent& e) {
  auto it = std::find_if(mViewports.begin(), mViewports.end(), [&e](const Viewport& v) { return v.getName() == e.mViewport.getName(); });
  if(it != mViewports.end())
    *it = e.mViewport;
  mViewports.push_back(e.mViewport);
}

void GraphicsSystem::_processRemoveViewportEvent(const RemoveViewportEvent& e) {
  auto it = std::find_if(mViewports.begin(), mViewports.end(), [&e](const Viewport& v) { return v.getName() == e.mName; });
  if(it != mViewports.end())
    mViewports.erase(it);
}

void GraphicsSystem::_processPickRequests(const std::vector<uint8_t> pickScene, const std::vector<ScreenPickRequest>& requests) {
  std::unordered_set<Handle> foundHandles;
  for(const ScreenPickRequest& req : requests) {
    Syx::Vec2 reqMin = req.mMin;
    Syx::Vec2 reqMax = req.mMax;
    //Flip y from top left to bottom left
    reqMin.y = mScreenSize.y - reqMin.y;
    reqMax.y = mScreenSize.y - reqMax.y;

    size_t min[2];
    size_t max[2];
    for(size_t i = 0; i < 2; ++i) {
      min[i] = static_cast<size_t>(std::min(reqMin[i], reqMax[i]));
      max[i] = static_cast<size_t>(std::max(reqMin[i], reqMax[i]));
    }

    //rgba
    const int pixelStride = 4;
    const int rowStride = pixelStride*static_cast<int>(mScreenSize.x);
    Handle lastFound = 0;
    for(size_t x = min[0]; x <= max[0]; ++x) {
      for(size_t y = min[1]; y <= max[1]; ++y) {
        size_t index = x*pixelStride + y*rowStride;
        if(index < pickScene.size()) {
          const uint8_t* pixel = &pickScene[index];
          if(Handle h = decodeHandle(pixel)) {
            //Set lookup is slower than last check, which is very likely in drag selection
            if(h != lastFound) {
              foundHandles.insert(h);
              lastFound = h;
            }
          }
        }
      }
    }

    std::vector<Handle> results;
    results.reserve(foundHandles.size());
    for(Handle h : foundHandles) {
      results.push_back(h);
    }
    mArgs.mMessages->getMessageQueue().get().push(ScreenPickResponse(req.mRequestId, req.mSpace, std::move(results)));
    foundHandles.clear();
  }
}

Camera& GraphicsSystem::_getActiveCamera() {
  return *_getCamera(mActiveCamera);
}

Camera* GraphicsSystem::_getCamera(Handle handle) {
  auto it = std::find_if(mCameras.begin(), mCameras.end(), [handle](const Camera& c) { return c.getOps().mOwner == handle; });
  return it != mCameras.end() ? &*it : nullptr;
}

Viewport* GraphicsSystem::_getViewport(const std::string& name) {
  auto it = std::find_if(mViewports.begin(), mViewports.end(), [&name](const Viewport& v) { return v.getName() == name; });
  return it != mViewports.end() ? &*it : nullptr;
}

void GraphicsSystem::onResize(int width, int height) {
  glViewport(0, 0, width, height);
  mScreenSize = Syx::Vec2(static_cast<float>(width), static_cast<float>(height));

  TextureDescription desc(width, height, TextureFormat::RGBA8, TextureSampleMode::Nearest);
  mFrameBuffer = std::make_unique<FrameBuffer>(desc);
  mPixelPackBuffer = std::make_unique<PixelBuffer>(desc, PixelBuffer::Type::Pack);
}
