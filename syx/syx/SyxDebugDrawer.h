#pragma once
#include "SyxVec3.h"

namespace Syx {
  namespace CommandType {
    enum {
      SetColor,
      DrawLine,
      DrawVector,
      DrawSphere,
      DrawCube,
      DrawPoint,
      DrawArc,
      DrawEllipse
    };
  }

  struct Command {
    Command(int type): mType(type) {}

    void Run(void);

    int mType;
    Vec3 mA, mB, mC, mD;
  };

  //Since physics is updated at a different rate than graphics, this will keep submitting
  class DebugDrawer {
  public:
    static DebugDrawer& Get(void);

    void SetColor(float r, float g, float b);
    void DrawLine(const Vec3& start, const Vec3& end);
    void DrawVector(const Vec3& start, const Vec3& direction);
    void DrawSphere(const Vec3& center, float radius, const Vec3& right, const Vec3& up);
    //Size is whole size, not half size
    void DrawCube(const Vec3& center, const Vec3& size, const Vec3& right, const Vec3& up);
    //Simple representation of a point, like a cross where size is the length from one side to the other
    void DrawPoint(const Vec3& point, float size);
    void DrawArc(const Vec3& point, const Vec3& begin, const Vec3& normal, float angle, float spiral = 0.0f);
    //X and Y are scaled vectors from center to right and upper parts of ellipse
    void DrawEllipse(const Vec3& center, const Vec3& x, const Vec3& y);

    void Draw(void);
    void Clear(void);
  private:
    std::vector<Command> mCommands;
  };
}