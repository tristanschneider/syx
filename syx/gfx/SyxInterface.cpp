#include "Precompile.h"
#include <SyxInterface.h>
#include <stdlib.h>
#include "DebugDrawer.h"

namespace Syx {
  /*struct SyxOptions {
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
  };*/

  namespace Interface {
    ::DebugDrawer* gDrawer = nullptr;

    SyxOptions GetOptions(void) {
      SyxOptions result;
      result.mDebugFlags = SyxOptions::DrawModels;
      result.mSimdFlags = 0;
      return result;
    }

    void SetColor(float r, float g, float b) {
      gDrawer->setColor(Vec3(r, g, b));
    }

    void DrawLine(const Vec3& start, const Vec3& end) {
      gDrawer->drawLine(start, end);
    }

    void DrawVector(const Vec3& start, const Vec3& direction) {
      gDrawer->drawVector(start, direction);
    }

    void DrawSphere(const Vec3& center, float radius, const Vec3& right, const Vec3& up) {

    }

    // Size is whole size, not half size
    void DrawCube(const Vec3& center, const Vec3& size, const Vec3& right, const Vec3& up) {

    }

    // Simple representation of a point, like a cross where size is the length from one side to the other
    void DrawPoint(const Vec3& point, float size) {

    }

    // 16 byte aligned
    void* AllocAligned(size_t size) {
      return _aligned_malloc(size, 16);
    }

    void FreeAligned(void* p) {
      return _aligned_free(p);
    }

    void* Alloc(size_t size) {
      return malloc(size);
    }

    void Free(void* p) {
      return free(p);
    }

    void Log(const std::string& message) {
      printf(message.c_str());
    }
  }
}