#pragma once
class Shader;
class BufferAttribs;

class DebugDrawer {
public:
  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& color);

private:
  struct Vertex {
    float mPos[3];
    float mColor[3];
  };

  std::unique_ptr<Shader> mShader;
  std::unique_ptr<BufferAttribs> mAttribs;
  std::vector<Vertex> mVerts;
  //Size of buffer currently on gpu
  size_t mBufferSize;
};