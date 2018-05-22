#pragma once
#include "System.h"

class Shader;
class Camera;
class DebugDrawer;
class ImGuiImpl;
class Model;
class Texture;
class Event;
class TransformEvent;
class ComponentEvent;
class ClearSceneEvent;
class RenderableUpdateEvent;
class EventBuffer;
class App;
class Asset;
class AddComponentEvent;
class RemoveComponentEvent;
class DrawLineEvent;
class DrawVectorEvent;
class DrawPointEvent;
class DrawCubeEvent;
class DrawSphereEvent;
class SetComponentPropEvent;
class SetComponentPropsEvent;
struct RenderableData;

class GraphicsSystem : public System {
public:
  RegisterSystemH(GraphicsSystem);
  GraphicsSystem(const SystemArgs& args);
  ~GraphicsSystem();

  void init() override;
  void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

  Camera& getPrimaryCamera();
  DebugDrawer& getDebugDrawer();

  void dispatchToRenderThread(std::function<void()> func);

  void onResize(int width, int height);

private:
  struct LocalRenderable {
    LocalRenderable(Handle h = InvalidHandle);
    Handle getHandle() const;

    Handle mHandle;
    Syx::Mat4 mTransform;
    std::shared_ptr<Asset> mModel;
    std::shared_ptr<Asset> mDiffTex;
  };

  void _render(float dt);

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
  void _processClearSceneEvent(const ClearSceneEvent& e);

  void _processRenderThreadTasks();

  void _setFromData(LocalRenderable& renderable, const RenderableData& data);

  std::shared_ptr<Asset> mGeometry;
  std::unique_ptr<Camera> mCamera;
  std::unique_ptr<DebugDrawer> mDebugDrawer;
  std::unique_ptr<ImGuiImpl> mImGui;
  Syx::Vec2 mScreenSize;

  //Tasks queued for execution on render thread
  std::vector<std::function<void()>> mTasks;
  std::vector<std::function<void()>> mLocalTasks;
  std::mutex mTasksMutex;

  //Local state
  MappedBuffer<LocalRenderable> mLocalRenderables;
};