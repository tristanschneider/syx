#include "Precompile.h"
#include "DebugDrawer.h"

#include "GameMath.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "glm/geometric.hpp"

namespace DebugDrawer {
  void setPoint(DebugLineAdapter& debug, size_t i, const glm::vec2& point, const glm::vec3& color) {
    debug.points->at(i).mPos = point;
    debug.points->at(i).mColor = color;
  }

  void setLine(DebugLineAdapter& debug, size_t i, const glm::vec2& a, const glm::vec2& b, const glm::vec3& color) {
    setPoint(debug, i, a, color);
    setPoint(debug, i + 1, b, color);
  }

  void drawLine(DebugLineAdapter& debug, const glm::vec2& a, const glm::vec2& b, const glm::vec3& color) {
    setLine(debug, debug.modifier.addElements(2), a, b, color);
  }

  void drawDirectedLine(DebugLineAdapter& debug, const glm::vec2& a, const glm::vec2& b, const glm::vec3& color) {
    drawVector(debug, a, b - a, color);
  }

  void drawVector(DebugLineAdapter& debug, const glm::vec2& point, const glm::vec2& dir, const glm::vec3& color) {
    const float length = glm::length(dir);
    if(length < 0.0001f) {
      return;
    }

    const size_t p = debug.modifier.addElements(6);
    constexpr float tipSize = 0.25f;
    const glm::vec2 scaledDir = dir * (tipSize/length);
    const glm::vec2 lineEnd = point + dir;
    const glm::vec2 tipBase = lineEnd - scaledDir;
    const glm::vec2 ortho = Math::orthogonal(scaledDir);
    setLine(debug, p, point, lineEnd, color);
    setLine(debug, p + 2, lineEnd, tipBase + ortho, color);
    setLine(debug, p + 4, lineEnd, tipBase - ortho, color);
  }

  void drawPoint(DebugLineAdapter& debug, const glm::vec2& p, float size, const glm::vec3& color) {
    const size_t index = debug.modifier.addElements(4);
    const glm::vec2 x{ size, 0.0f };
    const glm::vec2 y{ 0.0f, size };
    setLine(debug, index, p - x, p + x, color);
    setLine(debug, index + 2, p - y, p + y, color);
  }
}
