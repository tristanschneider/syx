#include "Precompile.h"
#include "GraphicsSystem.h"
#include "Model.h"
#include "Shader.h"
#include "BufferAttribs.h"
#include "Camera.h"

using namespace Syx;

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

static std::unique_ptr<Shader> loadShadersFromFile(const std::string& vsPath, const std::string& psPath, std::string& vsBuffer, std::string& psBuffer) {
  vsBuffer.clear();
  psBuffer.clear();
  std::unique_ptr<Shader> result = std::make_unique<Shader>();
  readFile(vsPath, vsBuffer);
  readFile(psPath, psBuffer);
  if(vsBuffer.size() && psBuffer.size()) {
    result->load(vsBuffer, psBuffer);
  }
  else {
    if(vsBuffer.empty())
      printf("Vertex shader not found at %s\n", vsPath.c_str());
    if(psBuffer.empty())
      printf("Pixel shader not found at %s\n", psPath.c_str());
  }
  return result;
}

Model _createTestTriangle() {
  GLuint vertexArray, vertexBuffer;
  //Generate a vertex array name
  glGenVertexArrays(1, &vertexArray);
  //Bind this array so we can fill it in
  glBindVertexArray(vertexArray);
  GLfloat triBuff[] = {
      -1.0f, -1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      0.0f,  1.0f, 0.0f,
  };
  //Generate a vertex buffer name
  glGenBuffers(1, &vertexBuffer);
  //Bind vertexBuffer as "Vertex attributes"
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  //Upload triBuff to gpu as vertexBuffer
  glBufferData(GL_ARRAY_BUFFER, sizeof(triBuff), triBuff, GL_STATIC_DRAW);
  return Model(vertexBuffer, vertexArray);
}

GraphicsSystem::~GraphicsSystem() {
}

GraphicsSystem::GraphicsSystem()
  : mCamera(std::make_unique<Camera>(CameraOps(1.396f, 1.396f, 0.1f, 100.0f)))
  , mGeometryAttribs(std::make_unique<BufferAttribs>()) {
}

void GraphicsSystem::init() {
  std::string buffA, buffB;
  mGeometry = loadShadersFromFile("shaders/phong.vs", "shaders/phong.ps", buffA, buffB);

  Model tri = _createTestTriangle();
  mTriHandle = addModel(tri);

  Mat4 ct = mCamera->getTransform();
  ct.setTranslate(Vec3(0.0f, 0.0f, -3.0f));
  ct.setRot(Quat::LookAt(-Vec3::UnitZ));
  mCamera->setTransform(ct);

  //Position
  mGeometryAttribs->addAttrib(3, GL_FLOAT, 0);
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
  mHandleToModel[model.mHandle] = model;
  return model.mHandle;
}

void GraphicsSystem::_render() {
  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  Model& tri = mHandleToModel[mTriHandle];
  {
    BufferAttribs::Binder bab(*mGeometryAttribs);
    glBindBuffer(GL_ARRAY_BUFFER, tri.mVB);
    {
      Shader::Binder b(*mGeometry);

      static Vec3 camPos(0.0f, 0.0f, -3.0f);
      Mat4 camTransform = Mat4::transform(Vec3::Identity, Quat::LookAt(-Vec3::UnitZ), camPos);
      static Quat triRot = Quat::Identity;
      triRot *= Quat::AxisAngle(Vec3::UnitY, 0.01f);
      Mat4 triTransform = Mat4::transform(Vec3::Identity, triRot, Vec3::Zero);
      Mat4 mvp = mCamera->getWorldToView() * triTransform;

      glUniformMatrix4fv(mGeometry->getUniform("mvp"), 1, GL_FALSE, mvp.mData);
      glUniform3f(mGeometry->getUniform("uColor"), 1.0f, 0.0f, 0.0f);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      triTransform = Mat4::transform(triRot, Vec3(2.0f, 0.0f, 0.0f));
      mvp = mCamera->getWorldToView() * triTransform;
      glUniformMatrix4fv(mGeometry->getUniform("mvp"), 1, GL_FALSE, mvp.mData);
      glUniform3f(mGeometry->getUniform("uColor"), 0.0f, 1.0f, 0.0f);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }
  }
}