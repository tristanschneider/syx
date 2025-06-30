#pragma once

#include <glm/vec2.hpp>

namespace ConvexHull {
  struct Context {
    std::vector<float> angles;
    std::vector<uint32_t> temp, result;
  };
  struct GetPoints {
    const glm::vec2& operator()(uint32_t i) const {
      return input[i];
    };
    const std::vector<glm::vec2>& input;
  };
  //Computes the convex hull of the input points which can be supplied in any order
  //The result is a set of indices pointing at input which are the minimum number of points
  //along the boundary needed to form the convex hull in counterclockwise order
  void compute(const std::vector<glm::vec2>& input, Context& ctx);
}