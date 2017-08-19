#pragma once
#include "System.h"

class Shader;
class Camera;
class DebugDrawer;
class ModelLoader;
class TextureLoader;
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
  void update(float dt) override;
  void uninit() override;

  Camera& getPrimaryCamera();
  DebugDrawer& getDebugDrawer();

  Handle addModel(Model& model);
  Handle addModel(const std::string& filePath);
  void removeModel(Handle model);

  Handle addTexture(const std::string& filePath);
  void removeTexture(Handle texture);

  std::unique_ptr<Shader> _loadShadersFromFile(const std::string& vsPath, const std::string& psPath);

private:
  void _render();

  std::unique_ptr<Shader> mGeometry;
  std::unique_ptr<Camera> mCamera;
  std::unique_ptr<DebugDrawer> mDebugDrawer;
  std::unique_ptr<ModelLoader> mModelLoader;
  std::unique_ptr<TextureLoader> mTextureLoader;
  std::unique_ptr<TransformListener> mTransformListener;
  std::unordered_map<Handle, Model> mHandleToModel;
  std::unordered_map<Handle, Texture> mHandleToTexture;
  HandleGen mModelGen, mTextureGen;
  std::string mVSBuffer;
  std::string mPSBuffer;
};