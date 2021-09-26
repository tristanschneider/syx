#pragma once
#include "graphics/GraphicsTypes.h"

class Shader;
class AssetRepo;
class Asset;

struct IDebugDrawer {
  virtual ~IDebugDrawer() = default;

  virtual void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& colorA, const Syx::Vec3& colorB) = 0;
  virtual void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& color) = 0;
  //Draw with color most recently set by setColor
  virtual void drawLine(const Syx::Vec3& a, const Syx::Vec3& b) = 0;
  virtual void drawVector(const Syx::Vec3& point, const Syx::Vec3& dir) = 0;
  virtual void DrawSphere(const Syx::Vec3& center, float radius, const Syx::Vec3& right, const Syx::Vec3& up) = 0;
  // Size is whole size, not half size
  virtual void DrawCube(const Syx::Vec3& center, const Syx::Vec3& size, const Syx::Vec3& right, const Syx::Vec3& up) = 0;
  // Simple representation of a point, like a cross where size is the length from one side to the other
  virtual void DrawPoint(const Syx::Vec3& point, float size) = 0;
  virtual void setColor(const Syx::Vec3& color) = 0;
};

class DebugDrawer : public IDebugDrawer {
public:
  DebugDrawer(AssetRepo& repo);
  ~DebugDrawer();

  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& colorA, const Syx::Vec3& colorB) override;
  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b, const Syx::Vec3& color) override;
  void drawLine(const Syx::Vec3& a, const Syx::Vec3& b) override;
  void drawVector(const Syx::Vec3& point, const Syx::Vec3& dir) override;
  void DrawSphere(const Syx::Vec3& center, float radius, const Syx::Vec3& right, const Syx::Vec3& up) override;
  void DrawCube(const Syx::Vec3& center, const Syx::Vec3& size, const Syx::Vec3& right, const Syx::Vec3& up) override;
  void DrawPoint(const Syx::Vec3& point, float size) override;

  void setColor(const Syx::Vec3& color) override;

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

namespace DebugDrawerExt {
  //Get a debug drawer that doesn't do anything
  IDebugDrawer& getNullDrawer();
}