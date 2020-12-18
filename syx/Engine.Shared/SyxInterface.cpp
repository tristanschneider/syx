#include "Precompile.h"
#include <SyxInterface.h>
#include <stdlib.h>
#include "DebugDrawer.h"

namespace Syx {
  namespace Interface {
    ::IDebugDrawer* gDrawer = nullptr;

    SyxOptions getOptions(void) {
      SyxOptions result;
      result.mDebugFlags = SyxOptions::DrawModels;
      result.mSimdFlags = 0;
      return result;
    }

    void setColor(float r, float g, float b) {
      gDrawer->setColor(Vec3(r, g, b));
    }

    void drawLine(const Vec3& start, const Vec3& end) {
      gDrawer->drawLine(start, end);
    }

    void drawVector(const Vec3& start, const Vec3& direction) {
      gDrawer->drawVector(start, direction);
    }

    void drawSphere(const Vec3& center, float radius, const Vec3&, const Vec3&) {
      drawPoint(center, radius*2.0f);
    }

    // Size is whole size, not half size
    void drawCube(const Vec3& center, const Vec3& size, const Vec3& right, const Vec3& up) {
      Vec3 r = right*size.x;
      Vec3 u = up*size.y;
      Vec3 f = right.cross(up)*size.z;

      Vec3 lbl = center - (r + u + f)*0.5f;
      Vec3 lbr = lbl + r;
      Vec3 ltl = lbl + f;
      Vec3 ltr = lbr + f;

      Vec3 ubl = lbl + u;
      Vec3 ubr = lbr + u;
      Vec3 utl = ltl + u;
      Vec3 utr = ltr + u;

      drawLine(lbl, lbr);
      drawLine(lbr, ltr);
      drawLine(ltr, ltl);
      drawLine(ltl, lbl);

      drawLine(ubl, ubr);
      drawLine(ubr, utr);
      drawLine(utr, utl);
      drawLine(utl, ubl);

      drawLine(lbl, ubl);
      drawLine(lbr, ubr);
      drawLine(ltl, utl);
      drawLine(ltr, utr);
    }

    // Simple representation of a point, like a cross where size is the length from one side to the other
    void drawPoint(const Vec3& point, float size) {
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
    void* allocAligned(size_t size) {
      return _aligned_malloc(size, 16);
    }

    void freeAligned(void* p) {
      return _aligned_free(p);
    }

    void* allocUnaligned(size_t size) {
      return malloc(size);
    }

    void freeUnaligned(void* p) {
      return free(p);
    }

    void log(const std::string& message) {
      printf(message.c_str());
    }
  }
}