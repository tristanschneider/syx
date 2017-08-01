#pragma once
#include "System.h"

class BufferAttribs;
class Shader;
class Camera;
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
private:
  void _render();

  std::unique_ptr<Shader> mGeometry;
  std::unordered_map<int, Model> mHandleToModel;
  HandleGen mModelGen;
  Handle mTriHandle;
  std::unique_ptr<Camera> mCamera;
  std::unique_ptr<BufferAttribs> mGeometryAttribs;
};