#pragma once
class Shader;
class GraphicsSystem;
class AssetRepo;
class Asset;

class DebugDrawer {
public:
  DebugDrawer(AssetRepo& repo);
  ~DebugDrawer();

  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& colorA, const Syx::Vec3& colorB);
  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& color);
  //Draw with color most recently set by setColor
  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b);
  void drawVector(const Syx::Vec3& point, const Syx::Vec3& dir);
  void DrawSphere(const Syx::Vec3& center, float radius, const Syx::Vec3& right, const Syx::Vec3& up);
  // Size is whole size, not half size
  void DrawCube(const Syx::Vec3& center, const Syx::Vec3& size, const Syx::Vec3& right, const Syx::Vec3& up);
  // Simple representation of a point, like a cross where size is the length from one side to the other
  void DrawPoint(const Syx::Vec3& point, float size);

  void setColor(const Syx::Vec3& color);

  void _render(const Syx::Mat4& wvp);

private:
  struct Vertex {
    float mPos[3];
    float mColor[3];
  };

  //Requires buffer to be bound before calling
  void _resizeBuffer(size_t newSize);

  std::shared_ptr<Asset> mShader;
  std::vector<Vertex> mVerts;
  std::mutex mVertsMutex;
  //Size of buffer currently on gpu
  size_t mBufferSize;
  GLHandle mVAO;
  GLHandle mVBO;
  Syx::Vec3 mColor;
};