#pragma once
#include "Shader.h"
#include "Model.h"
#include "Camera.h"

class GraphicsSystem {
public:
  GraphicsSystem();

  void init();
  void update(float dt);
  void uninit();

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