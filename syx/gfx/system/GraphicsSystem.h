#pragma once
#include "System.h"

class Shader;
class Camera;
class DebugDrawer;
class ModelLoader;
class TextureLoader;
class ImGuiImpl;
struct Model;
struct Texture;
struct TransformListener;

class GraphicsSystem : public System {
public:
  GraphicsSystem();
  ~GraphicsSystem();

  SystemId getId() const override {
    return SystemId::Graphics;
  }

  void init() override;
  void update(float dt, IWorkerPool& pool, std::shared_ptr<TaskGroup> frameTask) override;
  void uninit() override;

  Camera& getPrimaryCamera();
  DebugDrawer& getDebugDrawer();

  Handle addModel(Model& model);
  Handle addModel(const std::string& filePath);
  void removeModel(Handle model);

  Handle addTexture(const std::string& filePath);
  void removeTexture(Handle texture);

  std::unique_ptr<Shader> _loadShadersFromFile(const std::string& vsPath, const std::string& psPath);

  void onResize(int width, int height);

private:
  void _render(float dt);

  std::unique_ptr<Shader> mGeometry;
  std::unique_ptr<Camera> mCamera;
  std::unique_ptr<DebugDrawer> mDebugDrawer;
  std::unique_ptr<ModelLoader> mModelLoader;
  std::unique_ptr<TextureLoader> mTextureLoader;
  std::unique_ptr<TransformListener> mTransformListener;
  std::unique_ptr<ImGuiImpl> mImGui;
  std::unordered_map<Handle, Model> mHandleToModel;
  std::unordered_map<Handle, Texture> mHandleToTexture;
  HandleGen mModelGen, mTextureGen;
  std::string mVSBuffer;
  std::string mPSBuffer;
  Syx::Vec2 mScreenSize;
};