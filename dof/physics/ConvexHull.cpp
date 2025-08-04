#include <Precompile.h>
#include <ConvexHull.h>

#include <math/Geometric.h>

namespace ConvexHull {
  //Find a reference point on the boundary to sort the other points relative to
  struct GetReferencePoint {
    bool operator()(const glm::vec2& l, const glm::vec2& r) const {
      return l.y == r.y ? l.x < r.x : l.y < r.y;
    }
  };

  //Angle that the vector from the reference point to the X axis makes
  //Since the point is on the bottom all angles will be in the domain [0,pi]
  struct ComputeReferenceAngle {
    float operator()(const glm::vec2& v) const {
      if(&v == &referencePoint) {
        //Always start on the reference point
        return std::numeric_limits<float>::lowest();
      }
      const glm::vec2 toV = v - referencePoint;
      const float len = glm::length(toV);
      const float dotX = toV.x;
      constexpr float e = 0.00001f;
      return len > e ?  1.f - dotX/len : 0.0f;
    };
    const glm::vec2& referencePoint;
  };

  struct SortByReferenceAngle {
    bool operator()(uint32_t l, uint32_t r) const {
      const float al = angles[l];
      const float ar = angles[r];
      //Use manhattan distance as a tiebreaker. These will be discarded during the scanning.
      if(al == ar) {
        return Geo::manhattanDistance(input[l]) < Geo::manhattanDistance(input[r]);
      }
      return al < ar;
    }

    const std::vector<float>& angles;
    const std::vector<glm::vec2>& input;
  };

  //Given the string of points oldest->query->latest, determines if `query` is a valid boundary point, which is if the angles in order are counterclockwise
  //No angle difference is then also ignored, meaning redundant boundary points are discarded
  bool isBoundaryPoint(const glm::vec2& latest, const glm::vec2& query, const glm::vec2& oldest) {
    const glm::vec2 oldEdge = query - oldest;
    const glm::vec2 newEdge = latest - query;
    const float ccwAngle = Geo::cross(oldEdge, newEdge);
    return ccwAngle > 0;
  }

  //graham scan algorithm
  void compute(const std::vector<glm::vec2>& input, Context& ctx) {
    const uint32_t size = static_cast<uint32_t>(input.size());
    ctx.angles.resize(size);
    ctx.temp.reserve(size);
    ctx.result.resize(size);
    //If it's empty then so is the hull
    if(!size) {
      return;
    }
    //Generate indices pointing at the input vector in the starting order
    std::generate(ctx.result.begin(), ctx.result.end(), [i = uint32_t{}]() mutable { return i++; });

    const glm::vec2& reference = *std::min_element(input.begin(), input.end(), GetReferencePoint{});
    //Store computed angles for use in sorting
    std::transform(input.begin(), input.end(), ctx.angles.begin(), ComputeReferenceAngle{ reference });
    std::sort(ctx.result.begin(), ctx.result.end(), SortByReferenceAngle{ ctx.angles, input });

    ctx.temp.clear();
    for(uint32_t i = 0; i < size; ++i) {
      const glm::vec2& current = input[ctx.result[i]];

      //Consider if `current` evalutes the current value on the top of the stack, pop until it doesn't
      while(ctx.temp.size() > 1) {
        const size_t top = ctx.temp.size() - 1;
        if(!isBoundaryPoint(current, input[ctx.temp[top]], input[ctx.temp[top - 1]])) {
          ctx.temp.pop_back();
          continue;
        }
        break;
      }
      //Top of the stack is now a confirmed boundary point, add current to the new top
      ctx.temp.push_back(ctx.result[i]);
    }

    //Result was coputed in temp, swap it over to result container
    std::swap(ctx.temp, ctx.result);
    //Scanning evalutes 3 points at a time.
    //If one point is left that is a hull
    //If 3 points are left that means they were evaluated as a proper boundary
    //If 2 points are left they might be duplicates, if so, discard
    if(ctx.result.size() == 2) {
      //Note this is numerically different than the way duplicates are discarded for the 3 point comparison
      if(input[ctx.result[0]] == input[ctx.result[1]]) {
        ctx.result.pop_back();
      }
    }
  }
}