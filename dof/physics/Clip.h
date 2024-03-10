#pragma once

#include "glm/vec2.hpp"

namespace Clip {
  struct ClipContext {
    std::vector<glm::vec2> temp;
    std::vector<glm::vec2> result;
  };
  //Sutherland-Hodgman Clip a against b and store the result in a
  //The lists are counter-clockwise wound point strips that wrap
  void clipShapes(const std::vector<glm::vec2>& a, const std::vector<glm::vec2>& b, ClipContext& context);
}