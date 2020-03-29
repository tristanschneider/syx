#pragma once
#include "System.h"
#include "MappedBuffer.h"
#include "Handle.h"

class AddComponentEvent;
class App;
class Asset;
class Camera;
class ClearSpaceEvent;
class ComponentEvent;
class DebugDrawer;
class DrawCubeEvent;
class DrawLineEvent;
class DrawPointEvent;
class DrawSphereEvent;
class DrawVectorEvent;
class Event;
class EventBuffer;
class FrameBuffer;
class FullScreenQuad;
class GetCameraRequest;
class ImGuiImpl;
class Model;
class PixelBuffer;
class RemoveComponentEvent;
class RemoveViewportEvent;
class RenderCommandEvent;
class RenderableUpdateEvent;
class ScreenPickRequest;
class SetComponentPropEvent;
class SetComponentPropsEvent;
class SetViewportEvent;
class Shader;
class Texture;
class TransformEvent;
class Viewport;

struct RenderCommand;
struct RenderableData;

class GraphicsSystem : public System {
public:
  GraphicsSystem(const SystemArgs& args);
  ~GraphicsSystem();

  void init() override;
  void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

  Camera& getPrimaryCamera();
  ::DebugDrawer& getDebugDrawer();

  void dispatchToRenderThread(std::function<void()> func);

  void onResize(int width, int height);

private:
  struct LocalRenderable {
    LocalRenderable(Handle h = InvalidHandle);
    Handle getHandle() const;

    Handle mHandle;
    Handle mSpace;
    Syx::Mat4 mTransform;
    std::shared_ptr<Asset> mModel;
    std::shared_ptr<Asset> mDiffTex;
  };

  void _render(const Camera& camera, const Viewport& viewport);
  void _renderCommands(const Camera& camera, const Viewport& viewport);
  void _outline(const RenderCommand& c, const Camera& camera, const Viewport& viewport);
  void _quad2d(const RenderCommand& c, const Camera& camera, const Viewport& viewport);

  void _processAddEvent(const AddComponentEvent& e);
  void _processRemoveEvent(const RemoveComponentEvent& e);
  void _processTransformEvent(const TransformEvent& e);
  void _processRenderableEvent(const RenderableUpdateEvent& e);
  void _processDebugDrawEvent(const DrawLineEvent& e);
  void _processDebugDrawEvent(const DrawVectorEvent& e);
  void _processDebugDrawEvent(const DrawPointEvent& e);
  void _processDebugDrawEvent(const DrawCubeEvent& e);
  void _processDebugDrawEvent(const DrawSphereEvent& e);
  void _processSetCompPropsEvent(const SetComponentPropsEvent& e);
  void _processClearSpaceEvent(const ClearSpaceEvent& e);
  void _processScreenPickRequest(const ScreenPickRequest& e);
  void _processRenderCommandEvent(const RenderCommandEvent& e);
  void _processSetViewportEvent(const SetViewportEvent& e);
  void _processRemoveViewportEvent(const RemoveViewportEvent& e);
  void _processGetCameraRequest(const GetCameraRequest& e);

  void _processRenderThreadTasks();

  void _setFromData(LocalRenderable& renderable, const RenderableData& data);

  void _drawTexture(const Texture& tex, const Syx::Vec2& origin, const Syx::Vec2& size);
  void _drawBoundTexture(const Syx::Vec2& origin, const Syx::Vec2& size);
  void _drawPickScene(const FrameBuffer& destination, const Camera& camera, const Viewport& viewport);
  void _processPickRequests(const std::vector<uint8_t> pickScene, std::vector<ScreenPickRequest>& requests, Handle cameraId);

  Camera* _getCamera(Handle handle);
  Viewport* _getViewport(const std::string& name);

  void _glViewport(const Viewport& viewport) const;
  Syx::Vec2 _pixelToNDC(const Syx::Vec2 point) const;

  std::shared_ptr<Shader> mGeometry;
  std::shared_ptr<Shader> mFSQShader;
  std::shared_ptr<Shader> mFlatColorShader;

  std::unique_ptr<::DebugDrawer> mDebugDrawer;
  std::unique_ptr<ImGuiImpl> mImGui;
  std::unique_ptr<FullScreenQuad> mFullScreenQuad;
  std::unique_ptr<FrameBuffer> mFrameBuffer;
  std::unique_ptr<PixelBuffer> mPixelPackBuffer;
  Syx::Vec2 mScreenSize;

  //Tasks queued for execution on render thread
  std::vector<std::function<void()>> mTasks;
  std::vector<std::function<void()>> mLocalTasks;
  std::mutex mTasksMutex;

  std::vector<ScreenPickRequest> mPickRequests;

  //Local state
  MappedBuffer<LocalRenderable> mLocalRenderables;
  std::vector<RenderCommand> mRenderCommands;

  std::vector<Camera> mCameras;
  std::vector<Viewport> mViewports;
};