#include "Precompile.h"
#include "DebugDrawer.h"

#include "asset/Shader.h"
#include <gl/glew.h>

namespace {
  const int sStartingSize = 1000;
}

const AssetInfo DebugDrawer::SHADER_ASSET("shaders/debug.vs");

DebugDrawer::DebugDrawer(std::shared_ptr<Asset> shader) {
  mShader = std::move(shader);

  //Generate a vertex buffer name
  glGenBuffers(1, &mVBO);
  //Bind vertexBuffer as "Vertex attributes"
  glBindBuffer(GL_ARRAY_BUFFER, mVBO);
  _resizeBuffer(sStartingSize);

  //Generate a vertex array name
  glGenVertexArrays(1, &mVAO);
  //Bind this array so we can fill it in
  glBindVertexArray(mVAO);

  glEnableVertexAttribArray(0);
  //Interleave position and color
  //Position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
  //Color
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(float)*3));

  glBindVertexArray(0);
}

DebugDrawer::~DebugDrawer() {
}

void DebugDrawer::drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& colorA, const Syx::Vec3& colorB) {
  std::unique_lock<std::mutex> vLock(mVertsMutex);
  Vertex vA, vB;
  for(int i = 0; i < 3; ++i) {
    vA.mPos[i] = a[i];
    vB.mPos[i] = b[i];
    vA.mColor[i] = colorA[i];
    vB.mColor[i] = colorB[i];
  }
  mVerts.push_back(vA);
  mVerts.push_back(vB);
}

void DebugDrawer::drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& color) {
  drawLine(a, b, color, color);
}

void DebugDrawer::drawLine(const Syx::Vec3& a, const Syx::Vec3& b) {
  drawLine(a, b, mColor);
}

void DebugDrawer::drawVector(const Syx::Vec3& point, const Syx::Vec3& dir) {
  float tipSize = 0.1f;
  Syx::Vec3 ortho = dir.safeNormalized().getOrthogonal();
  Syx::Vec3 end = point + dir;
  drawLine(point, end);
  drawLine(end, end - dir*tipSize + ortho*tipSize);
}

void DebugDrawer::DrawSphere(const Syx::Vec3&, float, const Syx::Vec3&, const Syx::Vec3&) {

}

void DebugDrawer::DrawCube(const Syx::Vec3&, const Syx::Vec3&, const Syx::Vec3&, const Syx::Vec3&) {

}

void DebugDrawer::DrawPoint(const Syx::Vec3& point, float size) {
  float hSize = size*0.5f;
  for(int i = 0; i < 3; ++i) {
    Syx::Vec3 start, end;
    start = end = point;
    start[i] -= hSize;
    end[i] += hSize;
    drawLine(start, end);
  }
}

void DebugDrawer::setColor(const Syx::Vec3& color) {
  mColor = color;
}

void DebugDrawer::_render(const Syx::Mat4& wvp) {
  std::unique_lock<std::mutex> vLock(mVertsMutex);
  auto* shader = mShader->cast<Shader>();
  if(!shader || mVerts.empty() || shader->getState() != AssetState::PostProcessed) {
    return;
  }

  //Bind buffer so we can update it
  glBindBuffer(GL_ARRAY_BUFFER, mVBO);
  //Resize buffer if necessary
  if(mVerts.size() > mBufferSize) {
    _resizeBuffer(mVerts.size()*2);
  }

  //Upload latest lines to buffer
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex)*mVerts.size(), mVerts.data());

  //Draw lines
  glBindVertexArray(mVAO);
  {
    Shader::Binder b(*shader);
    glUniformMatrix4fv(shader->getUniform("wvp"), 1, GL_FALSE, wvp.mData);
    glDrawArrays(GL_LINES, 0, GLsizei(mVerts.size()));
  }
  glBindVertexArray(0);

  mVerts.clear();
}

void DebugDrawer::_resizeBuffer(size_t newSize) {
  //Upload to gpu as vertexBuffer
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*newSize, nullptr, GL_DYNAMIC_DRAW);
  mBufferSize = newSize;
}

IDebugDrawer& DebugDrawerExt::getNullDrawer() {
  struct Empty : public IDebugDrawer {
    void drawLine(const Syx::Vec3&, const Syx::Vec3&, const Syx::Vec3&, const Syx::Vec3&) {}
    void drawLine(const Syx::Vec3&, const Syx::Vec3&, const Syx::Vec3&) {}
    void drawLine(const Syx::Vec3&, const Syx::Vec3&) {}
    void drawVector(const Syx::Vec3&, const Syx::Vec3&) {}
    void DrawSphere(const Syx::Vec3&, float, const Syx::Vec3&, const Syx::Vec3&) {}
    void DrawCube(const Syx::Vec3&, const Syx::Vec3&, const Syx::Vec3&, const Syx::Vec3&) {}
    void DrawPoint(const Syx::Vec3&, float) {}
    void setColor(const Syx::Vec3&) {}
  };
  static Empty singleton;
  return singleton;
}