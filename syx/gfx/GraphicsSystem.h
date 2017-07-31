#pragma once
#include "Shader.h"
#include "Model.h"
#include "Camera.h"
#include "System.h"

class GraphicsSystem : public System {
public:
  GraphicsSystem();

  SystemId getId() const override {
    return SystemId::Graphics;
  }

  void init() override;
  void update(float dt) override;
  void uninit() override;

  Camera& getPrimaryCamera();

  Handle addModel(Model& model);
private:
  void _render();

  Shader mGeometry;
  std::unordered_map<int, Model> mHandleToModel;
  HandleGen mModelGen;
  Handle mTriHandle;
  Camera mCamera;
};