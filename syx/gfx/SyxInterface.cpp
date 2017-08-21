#include "Precompile.h"
#include <SyxInterface.h>
#include <stdlib.h>
#include "DebugDrawer.h"

namespace Syx {
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
      DrawPoint(center, radius*2.0f);
    }

    // Size is whole size, not half size
    void DrawCube(const Vec3& center, const Vec3& size, const Vec3& right, const Vec3& up) {
      Vec3 r = right*size.x;
      Vec3 u = up*size.y;
      Vec3 f = right.Cross(up)*size.z;

      Vec3 lbl = center - (r + u + f)*0.5f;
      Vec3 lbr = lbl + r;
      Vec3 ltl = lbl + f;
      Vec3 ltr = lbr + f;

      Vec3 ubl = lbl + u;
      Vec3 ubr = lbr + u;
      Vec3 utl = ltl + u;
      Vec3 utr = ltr + u;

      DrawLine(lbl, lbr);
      DrawLine(lbr, ltr);
      DrawLine(ltr, ltl);
      DrawLine(ltl, lbl);

      DrawLine(ubl, ubr);
      DrawLine(ubr, utr);
      DrawLine(utr, utl);
      DrawLine(utl, ubl);

      DrawLine(lbl, ubl);
      DrawLine(lbr, ubr);
      DrawLine(ltl, utl);
      DrawLine(ltr, utr);
    }

    // Simple representation of a point, like a cross where size is the length from one side to the other
    void DrawPoint(const Vec3& point, float size) {
      float hSize = size*0.5f;
      for(int i = 0; i < 3; ++i) {
        Vec3 start, end;
        start = end = point;
        start[i] -= hSize;
        end[i] += hSize;
        gDrawer->drawLine(start, end);
      }
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