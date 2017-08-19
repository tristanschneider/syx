#include "Precompile.h"
#include "DebugDrawer.h"
#include "Shader.h"
#include "system/GraphicsSystem.h"

const static int sStartingSize = 1000;

DebugDrawer::DebugDrawer(GraphicsSystem& graphics) {
  mShader = graphics._loadShadersFromFile("shaders/debug.vs", "shaders/debug.ps");

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
  Syx::Vec3 ortho = dir.SafeNormalized().GetOrthogonal();
  Syx::Vec3 end = point + dir;
  drawLine(point, end);
  drawLine(end, end - dir*tipSize + ortho*tipSize);
}

void DebugDrawer::DrawSphere(const Syx::Vec3& center, float radius, const Syx::Vec3& right, const Syx::Vec3& up) {

}

void DebugDrawer::DrawCube(const Syx::Vec3& center, const Syx::Vec3& size, const Syx::Vec3& right, const Syx::Vec3& up) {

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
  if(mVerts.empty())
    return;

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
    Shader::Binder b(*mShader);
    glUniformMatrix4fv(mShader->getUniform("wvp"), 1, GL_FALSE, wvp.mData);
    glDrawArrays(GL_LINES, 0, mVerts.size());
  }
  glBindVertexArray(0);

  mVerts.clear();
}

void DebugDrawer::_resizeBuffer(size_t newSize) {
  //Upload to gpu as vertexBuffer
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*newSize, nullptr, GL_DYNAMIC_DRAW);
  mBufferSize = newSize;
}