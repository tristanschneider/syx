#include "Precompile.h"
#include "SyxDebugDrawer.h"

namespace Syx {
  static void RunArc(const Vec3& point, const Vec3& start, const Vec3& normal, float angle, float spiral) {
    float segSize = SYX_2_PI/30.0f;
    Vec3 n = normal;
    if(angle < 0.0f) {
      n = -n;
      angle = -angle;
      spiral = -spiral;
    }
    Mat3 rot = Mat3::axisAngle(n, segSize);
    Vec3 last = start;
    Vec3 p = point;
    while(angle > segSize) {
      Vec3 cur = rot*last;
      Vec3 nextP = p + normal*(spiral*segSize);
      Interface::drawLine(p + last, nextP + cur);
      angle -= segSize;
      p = nextP;
      last = cur;
    }

    if(angle > 0.0f) {
      Vec3 cur = Mat3::axisAngle(n, angle)*last;
      Interface::drawLine(p + last, p + cur);
    }
  }

  static void RunEllipse(const Vec3& center, const Vec3& x, const Vec3& y) {
    //Given equation for ellipse 1 = (x*x)/(a*a) + (y*y)/(b*b) Input range of x values and solve for y
    //y = +-(b*sqrt(a*a - x*x))/a
    float a2 = x.length2();
    float a = std::sqrt(a2);
    float invA = 1.0f/a;
    //Should be even so point lands on tip of ellipse
    const int segments = 10;
    //Values along upper arc of ellipse
    float posX[segments];
    float posY[segments];
    float dx = 2.0f*a/static_cast<float>(segments + 1);
    float curX = -a;
    for(int i = 0; i < segments; ++i) {
      curX += dx;
      //We're going to multiply with x and y, so multiply in lengths to normalize it
      //x/vx.length() = x/a
      posX[i] = curX*invA;
      //y/vy.length() = ((b*sqrt(a*a - x*x)/a)/b
      posY[i] = std::sqrt(a2 - curX*curX)*invA;
    }

    Vec3 last = -x;
    for(int i = 0; i < segments; ++i) {
      Vec3 cur = x*posX[i] + y*posY[i];
      //Draw segment on upper arc, then lower arc
      Interface::drawLine(center + last, center + cur);
      Interface::drawLine(center - last, center - cur);
      last = cur;
    }
    Interface::drawLine(center + last, center + x);
    Interface::drawLine(center - last, center - x);
  }

  void Command::run(void) {
    switch(mType) {
      case CommandType::DrawCube: Interface::drawCube(mA, mB, mC, mD); break;
      case CommandType::DrawLine: Interface::drawLine(mA, mB); break;
      case CommandType::DrawPoint: Interface::drawPoint(mA, mB.x); break;
      case CommandType::DrawSphere: Interface::drawSphere(mA, mB.x, mC, mD); break;
      case CommandType::DrawVector: Interface::drawVector(mA, mB); break;
      case CommandType::SetColor: Interface::setColor(mA.x, mA.y, mA.z); break;
      case CommandType::DrawArc: RunArc(mA, mB, mC, mD.x, mD.y); break;
      case CommandType::DrawEllipse: RunEllipse(mA, mB, mC); break;
    }
  }

  void DebugDrawer::setColor(float r, float g, float b) {
    Command c(CommandType::SetColor);
    c.mA = Vec3(r, g, b);
    mCommands.push_back(c);
  }

  void DebugDrawer::drawLine(const Vec3& start, const Vec3& end) {
    Command c(CommandType::DrawLine);
    c.mA = start;
    c.mB = end;
    mCommands.push_back(c);
  }

  void DebugDrawer::drawVector(const Vec3& start, const Vec3& direction) {
    Command c(CommandType::DrawVector);
    c.mA = start;
    c.mB = direction;
    mCommands.push_back(c);
  }

  void DebugDrawer::drawSphere(const Vec3& center, float radius, const Vec3& right, const Vec3& up) {
    Command c(CommandType::DrawSphere);
    c.mA = center;
    c.mB = Vec3(radius);
    c.mC = right;
    c.mD = up;
    mCommands.push_back(c);
  }

  void DebugDrawer::drawCube(const Vec3& center, const Vec3& size, const Vec3& right, const Vec3& up) {
    Command c(CommandType::DrawCube);
    c.mA = center;
    c.mB = size;
    c.mC = right;
    c.mD = up;
    mCommands.push_back(c);
  }

  void DebugDrawer::drawPoint(const Vec3& point, float size) {
    Command c(CommandType::DrawPoint);
    c.mA = point;
    c.mB = Vec3(size);
    mCommands.push_back(c);
  }

  void DebugDrawer::drawArc(const Vec3& point, const Vec3& begin, const Vec3& normal, float angle, float spiral) {
    Command c(CommandType::DrawArc);
    c.mA = point;
    c.mB = begin;
    c.mC = normal;
    c.mD.x = angle;
    c.mD.y = spiral;
    mCommands.push_back(c);
  }

  void DebugDrawer::drawEllipse(const Vec3& center, const Vec3& x, const Vec3& y) {
    Command c(CommandType::DrawEllipse);
    c.mA = center;
    c.mB = x;
    c.mC = y;
    mCommands.push_back(c);
  }

  void DebugDrawer::draw(void) {
    for(Command& command : mCommands)
      command.run();
  }

  void DebugDrawer::clear(void) {
    mCommands.clear();
  }

  DebugDrawer& DebugDrawer::get(void) {
    static DebugDrawer singleton;
    return singleton;
  }
}