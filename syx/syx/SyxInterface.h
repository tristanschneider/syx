#pragma once
#include <string>

namespace Syx {
  struct Vector3;

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

    int mSimdFlags;
    int mDebugFlags;
    int mTest;
  };

  namespace Interface {
    SyxOptions GetOptions(void);

    void SetColor(float r, float g, float b);
    void DrawLine(const Vector3& start, const Vector3& end);
    void DrawVector(const Vector3& start, const Vector3& direction);
    void DrawSphere(const Vector3& center, float radius, const Vector3& right, const Vector3& up);
    // Size is whole size, not half size
    void DrawCube(const Vector3& center, const Vector3& size, const Vector3& right, const Vector3& up);
    // Simple representation of a point, like a cross where size is the length from one side to the other
    void DrawPoint(const Vector3& point, float size);

    // 16 byte aligned
    void* AllocAligned(size_t size);
    void FreeAligned(void* p);

    void* Alloc(size_t size);
    void Free(void* p);

    // Different types of allocations?

    // Maybe put logs and asserts here too
    void Log(const std::string& message);
  }
}