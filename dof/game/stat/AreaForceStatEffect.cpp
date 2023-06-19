#include "Precompile.h"
#include "stat/AreaForceStatEffect.h"

#include "Simulation.h"
#include "TableAdapters.h"

#include "glm/gtx/norm.hpp"
#include "GameMath.h"

#include "glm/glm.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/matrix_inverse.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "DebugDrawer.h"

namespace StatEffect {
  using namespace AreaForceStatEffect;

  struct FragmentForceEdge {
    glm::vec2 point{};
    float contribution{};
  };

  FragmentForceEdge _getEdge(const glm::vec2& normalizedForceDir, const glm::vec2& fragmentNormal, const glm::vec2& fragmentPos, float size) {
    const float cosAngle = glm::dot(normalizedForceDir, fragmentNormal);
    //Constribution is how close to aligned the force and normal are, point is position then normal in direction of force
    if(cosAngle >= 0.0f) {
      return { fragmentPos - fragmentNormal*size, 1.0f - cosAngle };
    }
    return { fragmentPos + fragmentNormal*size, 1.0f + cosAngle };
  }

  float crossProduct(const glm::vec2& a, const glm::vec2& b) {
    //[ax] x [bx] = [ax*by - ay*bx]
    //[ay]   [by]
    return a.x*b.y - a.y*b.x;
  }

  struct HitResult {
    UnpackedDatabaseElementID id;
    float remainingPiercing{};
    //Is average if there were multiple
    glm::vec2 hitPoint{};
    size_t hitCount{};
    glm::vec2 impulse{};
  };

  struct ShapeResult {
    UnpackedDatabaseElementID id;
    glm::vec2 pos{};
    glm::vec2 extentX{};
    glm::vec2 extentY{};
    float distance{};
    bool isTerrain{};

    //For Heap with closest distance first
    bool operator<(const ShapeResult& rhs) const {
      return distance > rhs.distance;
    }
  };

  struct Ray {
    glm::vec2 origin{};
    glm::vec2 direction{};
    float remainingPiercingDynamic{};
    float remainingPiercingTerrain{};
  };
  //[1, 0, tx][rx, lx, 0][sx,  0, 0]
  //[0, 1, ty][ry, ly, 0][ 0, sy, 0]
  //[0, 0,  1][0 ,  0, 1][ 0,  0, 1]
  //Scale*Rotation is this which is what the extents already are
  //[rx*sx, lx*sy, tx]
  //[ry*sx, ly*sy, ty]
  //[    0,     0,  1]
  glm::mat3 buildTransform(const glm::vec2& translate, const glm::vec2& extentX, const glm::vec2& extentY) {
    glm::mat3 result;
    result[0] = glm::vec3(extentX.x, extentX.y, 0.0f);
    result[1] = glm::vec3(extentY.x, extentY.y, 0.0f);
    result[2] = glm::vec3(translate.x, translate.y, 1);
    return result;
  }

  void castRays(std::vector<Ray>& rays, std::vector<ShapeResult>& shapes, std::vector<HitResult>& hits) {
    //Iterate over shapes sorted from closest to furthest
    //Using a heap can save time if the search stopped early due to the rays not piercing through all results
    std::make_heap(shapes.begin(), shapes.end());
    while(!shapes.empty()) {
      ShapeResult shape = shapes.front();
      std::pop_heap(shapes.begin(), shapes.end());
      shapes.pop_back();

      HitResult hit;
      hit.id = shape.id;
      //Transform rays into local space using this transform so that all intersections can be computed as aabb to line
      const glm::mat3 transform = buildTransform(shape.pos, shape.extentX, shape.extentY);
      const glm::mat3 invTransform = glm::affineInverse(transform);

      bool anyRaysLeft = false;
      for(Ray& ray : rays) {
        if(ray.remainingPiercingDynamic < 0.0f || ray.remainingPiercingTerrain < 0.0f) {
          continue;
        }
        anyRaysLeft = true;
        const glm::vec2 localRayBegin = Math::transformPoint(invTransform, ray.origin);
        const glm::vec2 localRayDirection = Math::transformVector(invTransform, ray.direction);
        [[maybe_unused]]auto asdf = Math::transformPoint(transform, localRayBegin);
        [[maybe_unused]]auto asdf2 = Math::transformVector(transform, localRayDirection);

        float tMin, tMax;
        if(Math::unitAABBLineIntersect(localRayBegin, localRayDirection, &tMin, &tMax)) {
          //I think due to scale the t values are only valid in local space, so the results need to be transformed back out
          hit.hitPoint += Math::transformPoint(transform, localRayBegin + localRayDirection*tMin);
          //Kind of like transforming the hit point, the exit point, getting their distance, but all in one step
          const float piercedAmount = glm::length(Math::transformVector(transform, localRayDirection*(tMax - tMin)));

          if(shape.isTerrain) {
            ray.remainingPiercingTerrain -= piercedAmount;
          }
          else {
            ray.remainingPiercingDynamic -= piercedAmount;
          }
          ++hit.hitCount;

          hit.impulse += ray.direction;
        }
      }
      if(!anyRaysLeft) {
        break;
      }
      //If something hit, add it to the results after averaging the values
      if(hit.hitCount) {
        const float inv = 1.0f/static_cast<float>(hit.hitCount);
        hit.hitPoint *= inv;
        hit.impulse *= inv;
        hits.push_back(hit);
      }
    }
  }

  void buildRaysForShape(const Command& command, const Command::Cone& cone, std::vector<Ray>& results) {
    if(!command.rayCount) {
      return;
    }
    //Start all the way on the right edge
    glm::vec2 ray = Math::rotate(command.direction, -cone.halfAngle);
    const float angleIncrement = cone.halfAngle*2.0f/static_cast<float>(command.rayCount);
    //Compute the increment that will advance from right to left to the other edge within rayCount steps
    const float cosAngle = std::cos(angleIncrement);
    const float sinAngle = std::sin(angleIncrement);
    for(size_t i = 0; i < command.rayCount; ++i) {
      results.push_back({ command.origin, ray });
      ray = Math::rotate(ray, cosAngle, sinAngle);
    }
  }

  //Gather an unsorted list of objects that are inside or touching the shape
  void gatherResultsInShape(const Command& command, const Command::Cone& cone, GameDB db, std::vector<ShapeResult>& results) {
    Queries::viewEachRowWithTableID(db.db, [&](GameDatabase::ElementID id,
      const FloatRow<Tags::GPos, Tags::X>& posX,
      const FloatRow<Tags::GPos, Tags::Y>& posY,
      const FloatRow<Tags::GRot, Tags::CosAngle>& rotX,
      const FloatRow<Tags::GRot, Tags::SinAngle>& rotY,
      const IsFragment&) {
      const bool isTerrain = Queries::getRowInTable<IsImmobile>(db.db, id) != nullptr;
      const glm::vec2& coneBase = command.origin;
      const glm::vec2& coneDir = command.direction;
      const float inConeAngle = std::cos(cone.halfAngle);
      const float sinConeAngle = std::abs(std::sin(cone.halfAngle));
      //Objects outside of this length are discarded, so add a half extent's worth to that length so
      //objects touching the back edge are still included
      constexpr float objSize = 0.5f;
      constexpr float objSize2 = objSize*objSize;
      const float paddedConeLength = cone.length + 0.5f;
      const float coneLength2 = paddedConeLength*paddedConeLength;

      for(size_t i = 0; i < posX.size(); ++i) {
        const glm::vec2 pos{ posX.at(i), posY.at(i) };
        const glm::vec2 toTarget = pos - coneBase;
        const float len2 = glm::length2(toTarget);
        if(len2 > coneLength2) {
          continue;
        }
        //If object is within objSize2 then it is inside the origin of the cone and should be included in results
        if(len2 > objSize2) {
          const float targetDot = glm::dot(toTarget, coneDir);
          const glm::vec2 targetDir = toTarget * (1.0f/std::sqrt(len2));
          //Cos of the angle is the dot product times the length of both, length of cone dir is one
          const float targetAngle = targetDot/std::sqrt(len2);
          //If the mid point isn't in the cone
          if(targetAngle < inConeAngle) {
            //Get the projection of the object onto the line through the center of the cone
            //Since coneDir is normalized this means it's also the distance along the cone,
            //which is the length of the adjecent edge of the triangle. The length of the opposite
            //edge is how far away this object can be to still be touching the cone
            const glm::vec2 projOnConeLine = coneDir*targetDot;
            const float distToConeLine2 = glm::length2(projOnConeLine - toTarget);
            //Get the length of the opposite edge of the triangle at the distance using the length obtained
            //from the adjacent edge
            const float allowedDistance = sinConeAngle*targetDot + objSize;
            if(distToConeLine2 > allowedDistance) {
              continue;
            }
          }
        }
        ShapeResult hit;
        hit.id = UnpackedDatabaseElementID::fromPacked(id).remakeElement(i);
        hit.pos = pos;
        hit.extentX = glm::vec2{ rotX.at(i), rotY.at(i) };// * 0.5f;
        //Since they're unit cubes this is the case, otherwise scale wouldn't be 0.5 for both
        hit.extentY = Math::orthogonal(hit.extentX);
        hit.isTerrain = isTerrain;
        hit.distance = len2;
        results.push_back(hit);
      }
    });
  }

  //Read position, write velocity
  TaskRange processStat(AreaForceStatEffectTable& table, GameDB db) {
    auto task = TaskNode::create([&table, db](...) {
      PROFILE_SCOPE("simulation", "forces");

      GameObjectAdapter obj = TableAdapters::getGameObjects(db);
      auto& cmd = std::get<CommandRow>(table.mRows);
      if(!cmd.size()) {
        return;
      }

      std::vector<ShapeResult> shapes;
      std::vector<HitResult> hits;
      std::vector<Ray> rays;
      for(size_t i = 0; i < cmd.size(); ++i) {
        const Command& command = cmd.at(i);
        shapes.clear();
        hits.clear();
        rays.clear();

        std::visit([&](const auto& shape) {
          gatherResultsInShape(command, shape, db, shapes);
          buildRaysForShape(command, shape, rays);
        }, command.shape);
        for(Ray& ray : rays) {
          ray.remainingPiercingTerrain = command.terrainPiercing;
          ray.remainingPiercingDynamic = command.dynamicPiercing;
        }
        auto temp = shapes;
        castRays(rays, temp, hits);

        auto debug = TableAdapters::getDebugLines(db);
        for(const Ray& ray : rays) {
          DebugDrawer::drawVector(debug, ray.origin, ray.direction, glm::vec3(0, 1, 0));
        }
        for(const HitResult& hit : hits) {
          DebugDrawer::drawDirectedLine(debug, command.origin, hit.hitPoint, glm::vec3(1, 0, 0));
        }
        for(const ShapeResult& shape : shapes) {
          DebugDrawer::drawPoint(debug, shape.pos, 0.25f, glm::vec3(1, 1, 0));
        }
      }

      /*
      auto& forcePointX = std::get<PointX>(table.mRows);
      auto& forcePointY = std::get<PointY>(table.mRows);

      for(size_t i = 0; i < obj.stable->size(); ++i) {
        glm::vec2 right{ obj.transform.rotX->at(i), obj.transform.rotY->at(i) };
        glm::vec2 up{ right.y, -right.x };
        glm::vec2 fragmentPos{ obj.transform.posX->at(i), obj.transform.posY->at(i) };
        glm::vec2 fragmentLinVel{ obj.physics.linVelX->at(i), obj.physics.linVelY->at(i) };
        float fragmentAngVel = obj.physics.angVel->at(i);
        for(size_t f = 0; f < strength.size(); ++f) {
          glm::vec2 forcePos{ forcePointX.at(f), forcePointY.at(f) };
          glm::vec2 impulse = fragmentPos - forcePos;
          float distance = glm::length(impulse);
          if(distance < 0.0001f) {
            impulse = glm::vec2(1.0f, 0.0f);
            distance = 1.0f;
          }
          //Linear falloff for now
          //TODO: something like easing for more interesting forces
          const float scalar = strength.at(f)/distance;
          //Normalize and scale to strength
          const glm::vec2 impulseDir = impulse/distance;
          impulse *= scalar;

          //Determine point to apply force at. Realistically this would be something like the center of pressure
          //Computing that is confusing so I'll hack at it instead
          //Take the two leading edges facing the force direction, then weight them based on their angle against the force
          //If the edge is head on that means only the one edge would matter
          //If the two edges were exactly 45 degrees from the force direction then the center of the two edges is chosen
          const float size = 0.5f;
          FragmentForceEdge edgeA = _getEdge(impulseDir, right, fragmentPos, size);
          FragmentForceEdge edgeB = _getEdge(impulseDir, right, fragmentPos, size);
          const glm::vec2 impulsePoint = edgeA.point*edgeA.contribution + edgeB.point*edgeB.contribution;

          fragmentLinVel += impulse;
          glm::vec2 r = impulsePoint - fragmentPos;
          fragmentAngVel += crossProduct(r, impulse);
        }

        obj.physics.linVelX->at(i) = fragmentLinVel.x;
        obj.physics.linVelY->at(i) = fragmentLinVel.y;
        obj.physics.angVel->at(i) = fragmentAngVel;
      }*/
    });

    return TaskBuilder::addEndSync(task);
  }
}