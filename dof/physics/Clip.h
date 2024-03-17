#pragma once

#include "glm/vec2.hpp"

namespace Clip {
  struct ClipContext {
    std::vector<glm::vec2> temp;
    std::vector<glm::vec2> result;
  };
  struct StartAndDir {
    static StartAndDir fromStartEnd(const glm::vec2& start, const glm::vec2& end) {
      return { start, end - start };
    }

    glm::vec2 start{};
    glm::vec2 dir{};
  };

  struct LineLineIntersectTimes {
    std::optional<float> tA, tB;
  };

  LineLineIntersectTimes getIntersectTimes(const StartAndDir& a, const StartAndDir& b);
  std::optional<float> getIntersectA(const StartAndDir& a, const StartAndDir& b);

  //Sutherland-Hodgman Clip a against b and store the result in a
  //The lists are counter-clockwise wound point strips that wrap
  void clipShapes(const std::vector<glm::vec2>& a, const std::vector<glm::vec2>& b, ClipContext& context);
}