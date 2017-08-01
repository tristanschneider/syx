#pragma once
#include "System.h"

class Shader;
class Camera;
class DebugDrawer;
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
  std::unique_ptr<Shader> _loadShadersFromFile(const std::string& vsPath, const std::string& psPath);

private:
  void _render();

  std::unique_ptr<Shader> mGeometry;
  std::unique_ptr<Camera> mCamera;
  std::unique_ptr<DebugDrawer> mDebugDrawer;
  std::unordered_map<int, Model> mHandleToModel;
  HandleGen mModelGen;
  Handle mTriHandle;
  std::string mVSBuffer;
  std::string mPSBuffer;
};