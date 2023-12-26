#include "glm/glm.hpp"

namespace Dyn {
  //Looks strange in 2d since angular velocity float is the scale of a rotation vector that is always coming out of the screen
  constexpr glm::vec2 crossAngularVelocity(const glm::vec2& a, float velocity) {
    //[a.x]   [0]
    //[a.y] x [0]
    //[0]     [v]
    return { a.y*velocity, -a.x*velocity };
  }

  constexpr glm::vec2 velocityAtPoint(
    const glm::vec2& centerToPoint,
    const glm::vec2& linearVelocity,
    float angularVelocity
  ) {
    return linearVelocity + crossAngularVelocity(centerToPoint, angularVelocity);
  }
}