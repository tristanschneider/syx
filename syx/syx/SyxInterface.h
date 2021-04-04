#pragma once

namespace Syx {
  struct Vec3;

  struct SyxOptions {
    enum SIMD {
      PositionIntegration = 1 << 0,
      VelocityIntegration = 1 << 1,
      SupportPoints = 1 << 2,
      Transforms = 1 << 3,
      ConstraintSolve = 1 << 4,
      GJK = 1 << 5,
      EPA = 1 << 6
    };

    enum Debug {
      DrawModels = 1 << 0,
      DrawPersonalBBs = 1 << 1,
      DrawCollidingPairs = 1 << 2,
      DrawGJK = 1 << 3,
      DrawEPA = 1 << 4,
      DrawNewContact = 1 << 5,
      DrawManifolds = 1 << 6,
      DisableCollision = 1 << 7,
      DrawBroadphase = 1 << 8,
      DrawIslands = 1 << 9,
      DrawSleeping = 1 << 10,
      DrawJoints = 1 << 11
    };

    int mSimdFlags = 0;
    int mDebugFlags = 0;
    int mTest = 0;
  };

  namespace Interface {
    SyxOptions getOptions(void);

    void setColor(float r, float g, float b);
    void drawLine(const Vec3& start, const Vec3& end);
    void drawVector(const Vec3& start, const Vec3& direction);
    void drawSphere(const Vec3& center, float radius, const Vec3& right, const Vec3& up);
    // Size is whole size, not half size
    void drawCube(const Vec3& center, const Vec3& size, const Vec3& right, const Vec3& up);
    // Simple representation of a point, like a cross where size is the length from one side to the other
    void drawPoint(const Vec3& point, float size);

    // 16 byte aligned
    void* allocAligned(size_t size);
    void freeAligned(void* p);

    void* allocUnaligned(size_t size);
    void freeUnaligned(void* p);

    void log(const std::string& message);
  }
}