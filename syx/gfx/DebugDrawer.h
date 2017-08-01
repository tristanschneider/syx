#pragma once
class Shader;
class GraphicsSystem;

class DebugDrawer {
public:
  DebugDrawer(GraphicsSystem& graphics);
  ~DebugDrawer();

  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& colorA, const Syx::Vec3& colorB);
  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& color);
  //Draw with color most recently set by setColor
  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b);
  void setColor(const Syx::Vec3& color);

  void _render(const Syx::Mat4& wvp);

private:
  struct Vertex {
    float mPos[3];
    float mColor[3];
  };

  //Requires buffer to be bound before calling
  void _resizeBuffer(size_t newSize);

  std::unique_ptr<Shader> mShader;
  std::vector<Vertex> mVerts;
  //Size of buffer currently on gpu
  size_t mBufferSize;
  GLuint mVAO;
  GLuint mVBO;
  Syx::Vec3 mColor;
};