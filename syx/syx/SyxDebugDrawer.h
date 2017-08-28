#pragma once

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
    Command(int type)
      : mType(type) {
    }

    void run(void);

    int mType;
    Vec3 mA, mB, mC, mD;
  };

  //Since physics is updated at a different rate than graphics, this will keep submitting
  class DebugDrawer {
  public:
    static DebugDrawer& get(void);

    void setColor(float r, float g, float b);
    void drawLine(const Vec3& start, const Vec3& end);
    void drawVector(const Vec3& start, const Vec3& direction);
    void drawSphere(const Vec3& center, float radius, const Vec3& right, const Vec3& up);
    //Size is whole size, not half size
    void drawCube(const Vec3& center, const Vec3& size, const Vec3& right, const Vec3& up);
    //Simple representation of a point, like a cross where size is the length from one side to the other
    void drawPoint(const Vec3& point, float size);
    void drawArc(const Vec3& point, const Vec3& begin, const Vec3& normal, float angle, float spiral = 0.0f);
    //X and Y are scaled vectors from center to right and upper parts of ellipse
    void drawEllipse(const Vec3& center, const Vec3& x, const Vec3& y);

    void draw(void);
    void clear(void);
  private:
    std::vector<Command> mCommands;
  };
}