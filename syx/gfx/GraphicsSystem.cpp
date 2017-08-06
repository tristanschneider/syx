#include "Precompile.h"
#include "GraphicsSystem.h"
#include "Model.h"
#include "Shader.h"
#include "Camera.h"
#include "DebugDrawer.h"
#include "ModelLoader.h"

using namespace Syx;

static Handle sTestModel;

static void readFile(const std::string& path, std::string& buffer) {
  std::ifstream file(path, std::ifstream::in | std::ifstream::binary);
  if(file.good()) {
    file.seekg(0, file.end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, file.beg);
    buffer.resize(size + 1);
    file.read(&buffer[0], size);
    buffer[size] = 0;
  }
}

std::unique_ptr<Shader> GraphicsSystem::_loadShadersFromFile(const std::string& vsPath, const std::string& psPath) {
  mVSBuffer.clear();
  mPSBuffer.clear();
  std::unique_ptr<Shader> result = std::make_unique<Shader>();
  readFile(vsPath, mVSBuffer);
  readFile(psPath, mPSBuffer);
  if(mVSBuffer.size() && mPSBuffer.size()) {
    result->load(mVSBuffer, mPSBuffer);
  }
  else {
    if(mVSBuffer.empty())
      printf("Vertex shader not found at %s\n", vsPath.c_str());
    if(mPSBuffer.empty())
      printf("Pixel shader not found at %s\n", psPath.c_str());
  }
  return result;
}

GraphicsSystem::~GraphicsSystem() {
}

GraphicsSystem::GraphicsSystem() {
}

void GraphicsSystem::init() {
  mCamera = std::make_unique<Camera>(CameraOps(1.396f, 1.396f, 0.1f, 100.0f));
  mGeometry = _loadShadersFromFile("shaders/phong.vs", "shaders/phong.ps");

  Mat4 ct = mCamera->getTransform();
  ct.setTranslate(Vec3(0.0f, 0.0f, -3.0f));
  ct.setRot(Quat::LookAt(-Vec3::UnitZ));
  mCamera->setTransform(ct);

  mDebugDrawer = std::make_unique<DebugDrawer>(*this);

  mModelLoader = std::make_unique<ModelLoader>();

  sTestModel = addModel("models/car.obj");
}

void GraphicsSystem::update(float dt) {
  _render();
}

void GraphicsSystem::uninit() {
}

Camera& GraphicsSystem::getPrimaryCamera() {
  return *mCamera;
}

Handle GraphicsSystem::addModel(Model& model) {
  model.mHandle = mModelGen.Next();
  Model& added = mHandleToModel[model.mHandle] = model;
  //Ultimately this should be a separate step as needed
  added.loadGpu();
  return added.mHandle;
}

Handle GraphicsSystem::addModel(const std::string& filePath) {
  std::unique_ptr<Model> model = mModelLoader->loadModel(filePath);
  if(model) {
    return addModel(*model);
  }
  return InvalidHandle;
}

void GraphicsSystem::_render() {
  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  mDebugDrawer->_render(mCamera->getWorldToView());
  Model& testModel = mHandleToModel[sTestModel];
  {
    Shader::Binder b(*mGeometry);

    static Quat triRot = Quat::Identity;
    triRot *= Quat::AxisAngle(Vec3::UnitY, 0.01f);
    Mat4 triTransform = Mat4::transform(Vec3(0.1f), triRot, Vec3::Zero);
    Mat4 mvp = mCamera->getWorldToView() * triTransform;
    Vec3 camPos = mCamera->getTransform().getTranslate();

    Vec3 mDiff(0.3f);
    Vec3 mSpec(0.6f);
    mSpec.w = 2.5f;
    Vec3 mAmb(0.2f, 0.0f, 0.0f);

    Vec3 sunDir = -Vec3::Identity.Normalized();
    Vec3 sunColor = Vec3::Identity;
    {
      Vec3 p(3.0f);
      mDebugDrawer->drawLine(p, p + sunDir, sunColor);
      mDebugDrawer->drawLine(p + sunDir, p + sunDir - Vec3(0.1f));
    }

    glBindVertexArray(testModel.mVA);
    //This bind shouldn't be needed since vao is supposed to know all it needs, but some intel cards apparently have a bug with index buffers
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, testModel.mIB);
    glUniformMatrix4fv(mGeometry->getUniform("uMVP"), 1, GL_FALSE, mvp.mData);
    glUniformMatrix4fv(mGeometry->getUniform("uMW"), 1, GL_FALSE, triTransform.mData);
    glUniform3f(mGeometry->getUniform("uCamPos"), camPos.x, camPos.y, camPos.z);
    glUniform3f(mGeometry->getUniform("uDiffuse"), mDiff.x, mDiff.y, mDiff.z);
    glUniform3f(mGeometry->getUniform("uAmbient"), mAmb.x, mAmb.y, mAmb.z);
    glUniform4f(mGeometry->getUniform("uSpecular"), mSpec.x, mSpec.y, mSpec.z, mSpec.w);
    glUniform3f(mGeometry->getUniform("uSunDir"), sunDir.x, sunDir.y, sunDir.z);
    glUniform3f(mGeometry->getUniform("uSunColor"), sunColor.x, sunColor.y, sunColor.z);

    glDrawElements(GL_TRIANGLES, testModel.mIndices.size(), GL_UNSIGNED_INT, nullptr);
  }
}