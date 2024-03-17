#include "Precompile.h"
#include "Clip.h"

#include "Geometric.h"

namespace Clip {
  void clipShapes(const std::vector<glm::vec2>& a, const std::vector<glm::vec2>& b, ClipContext& context) {
    context.result.clear();
    if(b.empty() || a.empty()) {
      return;
    }
    const std::vector<glm::vec2>* input = &a;
    std::vector<glm::vec2>* output = &context.result;
    for(size_t clipEdge = 0; clipEdge <= b.size(); ++clipEdge) {
      const glm::vec2& edgeStart = b[clipEdge];
      const glm::vec2& edgeEnd = b[(clipEdge + 1) % b.size()];
      const glm::vec2 edgeNormal = Geo::orthogonal(edgeEnd - edgeStart);
      //Intersect on line C to L with edge of normal N with point on edge E is P
      //L + t*(C - L) = P
      //(P - E).N = 0
      //Substitue one into the other
      //(L + t(C - L)-e).N = 0
      //t = n.(E - L)/n.(C - L)
      if(Geo::nearZero(edgeNormal)) {
        continue;
      }

      output->clear();
      glm::vec2 lastPoint = input->at(input->size() - 1);
      float lastProj = glm::dot(lastPoint - edgeStart, edgeNormal);
      bool lastInside = lastProj <= 0.0f;
      for(size_t i = 0; i < input->size(); ++i) {
        const glm::vec2& current = input->at(i);
        const float currentProj = glm::dot(current - edgeStart, edgeNormal);
        const bool currentInside = currentProj <= 0.0f;
        if(currentInside) {
          //Crossed from outside to inside
          if(!lastInside) {
            const glm::vec2 lastToCurrent = current - lastPoint;
            //Divisor can't be zero because lastToCurrent would then need to be orthogonal to the normal
            //but if it was then the line wouldn't have crossed the normal
            const float t = -lastProj/glm::dot(lastToCurrent, edgeNormal);
            const glm::vec2 intersect = lastPoint + lastToCurrent*t;
            output->push_back(intersect);
          }
          output->push_back(current);
        }
        //Crossed from inside to outside
        else if(lastInside) {
          const glm::vec2 lastToCurrent = current - lastPoint;
          const float t = -lastProj/glm::dot(lastToCurrent, edgeNormal);
          const glm::vec2 intersect = lastPoint + lastToCurrent*t;
          output->push_back(intersect);
        }

        lastProj = currentProj;
        lastPoint = current;
        lastInside = currentInside;
      }

      if(output->empty()) {
        context.result.clear();
        return;
      }
      //Swap input and output. On the first iteration it's the 'a' container
      if(output == &context.result) {
        input = &context.result;
        output = &context.temp;
      }
      else {
        output = &context.result;
        input = &context.temp;
      }
      output->clear();
    }

    //End of iteration puts the points in input for the next iteration
    //This means at the end the result is in input
    auto finalResult = input;
    //Make sure final result is in context.result
    if(finalResult == &context.temp) {
      context.temp.swap(context.result);
    }
  }

  LineLineIntersectTimes getIntersectTimes(const StartAndDir& lA, const StartAndDir& lB) {
    //a1.x + t(a2.x - a1.x) = b1.x + s(b2.x - b1.x)
    //a1.y + t(a2.y - a1.y) = b1.y + s(b2.y - b1.y)
    //Rearrange to form Ax=b
    //t(a2.x - a1.x) - s(b2.x - b1.x) = b1.x - a1.x
    //t(a2.y - a1.y) - s(b2.y - b1.y) = b1.y - a1.y
    //Use Cramer's rule to solve
    //[a b]=[e]
    //[c d] [f]
    //t = (ed - fb)/(ad - bc)
    //s = (af - ce)/(ad - bc)
    const glm::vec2& col1 = lA.dir;
    const glm::vec2 col2 = -lB.dir;
    const glm::vec2 col3 = lB.start - lA.start;
    const float determinant = Geo::det(col1, col2);
    //Parallel lines
    if(std::abs(determinant) <= Geo::EPSILON) {
      return {};
    }
    const float t = Geo::det(col3, col2)/determinant;
    const float s = Geo::det(col1, col3)/determinant;
    return { t, s };
  }

  std::optional<float> getIntersectA(const StartAndDir& lA, const StartAndDir& lB) {
    const glm::vec2& col1 = lA.dir;
    const glm::vec2 col2 = -lB.dir;
    const glm::vec2 col3 = lB.start - lA.start;
    const float determinant = Geo::det(col1, col2);
    //Parallel lines
    if(std::abs(determinant) <= Geo::EPSILON) {
      return {};
    }
    return Geo::det(col3, col2)/determinant;
  }
};