#pragma once
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

struct DebugLineAdapter;

namespace DebugDrawer {
  void drawLine(DebugLineAdapter& debug, const glm::vec2& a, const glm::vec2& b, const glm::vec3& color);
  void drawDirectedLine(DebugLineAdapter& debug, const glm::vec2& a, const glm::vec2& b, const glm::vec3& color);
  void drawVector(DebugLineAdapter& debug, const glm::vec2& point, const glm::vec2& dir, const glm::vec3& color);
  void drawPoint(DebugLineAdapter& debug, const glm::vec2& p, float size, const glm::vec3& color);
  void drawText(DebugLineAdapter& debug, const glm::vec2& p, std::string text);
  void drawAABB(DebugLineAdapter& debug, const glm::vec2& min, const glm::vec2& max, const glm::vec3& color);
};
