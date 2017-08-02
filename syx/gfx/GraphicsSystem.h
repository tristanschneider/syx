#pragma once
#include "System.h"

class Shader;
class Camera;
class DebugDrawer;
class ModelLoader;
struct Model;

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

  Handle addModel(Model& model);
  Handle addModel(const std::string& filePath);
  std::unique_ptr<Shader> _loadShadersFromFile(const std::string& vsPath, const std::string& psPath);

private:
  void _render();

  std::unique_ptr<Shader> mGeometry;
  std::unique_ptr<Camera> mCamera;
  std::unique_ptr<DebugDrawer> mDebugDrawer;
  std::unique_ptr<ModelLoader> mModelLoader;
  std::unordered_map<int, Model> mHandleToModel;
  HandleGen mModelGen;
  std::string mVSBuffer;
  std::string mPSBuffer;
};