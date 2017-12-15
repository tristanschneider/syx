#pragma once
#include "System.h"

class Shader;
class Camera;
class DebugDrawer;
class TextureLoader;
class ImGuiImpl;
class Model;
struct Texture;
struct TransformListener;
class Event;
struct TransformEvent;
class ComponentEvent;
class RenderableUpdateEvent;
struct EventListener;
class App;
class Asset;

class GraphicsSystem : public System {
public:
  RegisterSystemH(GraphicsSystem);
  GraphicsSystem(App& app);
  ~GraphicsSystem();

  void init() override;
  void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

  Camera& getPrimaryCamera();
  DebugDrawer& getDebugDrawer();

  Handle addTexture(const std::string& filePath);
  void removeTexture(Handle texture);
  void dispatchToRenderThread(std::function<void()> func);

  void onResize(int width, int height);

private:
  struct LocalRenderable {
    LocalRenderable(Handle h = InvalidHandle);
    Handle getHandle() const;

    Handle mHandle;
    Syx::Mat4 mTransform;
    std::shared_ptr<Asset> mModel;
    Texture* mDiffTex;
  };

  void _render(float dt);
  void _processEvents();
  void _processAddEvent(const ComponentEvent& e);
  void _processRemoveEvent(const ComponentEvent& e);
  void _processTransformEvent(const TransformEvent& e);
  void _processRenderableEvent(const RenderableUpdateEvent& e);
  void _processRenderThreadTasks();

  std::shared_ptr<Asset> mGeometry;
  std::unique_ptr<Camera> mCamera;
  std::unique_ptr<DebugDrawer> mDebugDrawer;
  std::unique_ptr<TextureLoader> mTextureLoader;
  std::unique_ptr<ImGuiImpl> mImGui;
  std::unordered_map<Handle, Texture> mHandleToTexture;
  HandleGen mTextureGen;
  std::string mVSBuffer;
  std::string mPSBuffer;
  Syx::Vec2 mScreenSize;

  std::unique_ptr<TransformListener> mTransformListener;
  std::unique_ptr<EventListener> mEventListener;

  //Tasks queued for execution on render thread
  std::vector<std::function<void()>> mTasks;
  std::vector<std::function<void()>> mLocalTasks;
  std::mutex mTasksMutex;

  //Local state
  MappedBuffer<LocalRenderable> mLocalRenderables;
};