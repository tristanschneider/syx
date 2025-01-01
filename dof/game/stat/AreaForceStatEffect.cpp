#include "Precompile.h"
#include "stat/AreaForceStatEffect.h"

#include "DBEvents.h"
#include "Simulation.h"
#include "stat/AllStatEffects.h"
#include "stat/DamageStatEffect.h"
#include "TableAdapters.h"

#include "glm/gtx/norm.hpp"
#include "GameMath.h"

#include "glm/glm.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/matrix_inverse.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "AppBuilder.h"

#include "DebugDrawer.h"

namespace AreaForceStatEffect {
  RuntimeTable& getTable(AppTaskArgs& args) {
    return StatEffectDatabase::getStatTable<AreaForceStatEffect::CommandRow>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase{ getTable(args) }
  {
    command = table.tryGet<CommandRow>();
  }

  Builder& Builder::setOrigin(const glm::vec2& origin) {
    for(auto i : currentEffects) {
      command->at(i).origin = origin;
    }
    return *this;
  }

  Builder& Builder::setDirection(const glm::vec2& dir) {
    for(auto i : currentEffects) {
      command->at(i).direction = dir;
    }
    return *this;
  }

  Builder& Builder::setShape(const Command::Variant& shape) {
    for(auto i : currentEffects) {
      command->at(i).shape = shape;
    }
    return *this;
  }

  Builder& Builder::setPiercing(float dynamic, float terrain) {
    for(auto i : currentEffects) {
      command->at(i).dynamicPiercing = dynamic;
      command->at(i).terrainPiercing = terrain;
    }
    return *this;
  }

  Builder& Builder::setDamage(float damage) {
    for(auto i : currentEffects) {
      command->at(i).damage = damage;
    }
    return *this;
  }

  Builder& Builder::setRayCount(size_t count) {
    for(auto i : currentEffects) {
      command->at(i).rayCount = count;
    }
    return *this;
  }

  Builder& Builder::setDislodgeFragments() {
    for(auto i : currentEffects) {
      command->at(i).dislodgeFragments = true;
    }
    return *this;
  }

  Builder& Builder::setImpulse(Command::ImpulseType impulse) {
    for(auto i : currentEffects) {
      command->at(i).impulseType = impulse;
    }
    return *this;
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

  struct ShapesQuery {
    UnpackedDatabaseElementID table;
    const FloatRow<Tags::GPos, Tags::X>* posX{};
    const FloatRow<Tags::GPos, Tags::Y>* posY{};
    const FloatRow<Tags::GRot, Tags::CosAngle>* rotX{};
    const FloatRow<Tags::GRot, Tags::SinAngle>* rotY{};
    FloatRow<Tags::GLinImpulse, Tags::X>* impulseX{};
    FloatRow<Tags::GLinImpulse, Tags::Y>* impulseY{};
    FloatRow<Tags::GAngImpulse, Tags::Angle>* impulseA{};
    const IsImmobile* isImmobile{};
  };

  //TODO: replace with SpatialQueries
  //Gather an unsorted list of objects that are inside or touching the shape
  void gatherResultsInShape(const Command& command, const Command::Cone& cone, ShapesQuery& query, std::vector<ShapeResult>& results) {
    const bool isTerrain = query.isImmobile != nullptr;
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

    for(size_t i = 0; i < query.posX->size(); ++i) {
      const glm::vec2 pos{ query.posX->at(i), query.posY->at(i) };
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
      hit.id = query.table.remakeElement(i);
      hit.pos = pos;
      hit.extentX = glm::vec2{ query.rotX->at(i), query.rotY->at(i) };// * 0.5f;
      //TODO: this is probably an incorrect assumption now
      //Since they're unit cubes this is the case, otherwise scale wouldn't be 0.5 for both
      hit.extentY = Math::orthogonal(hit.extentX);
      hit.isTerrain = isTerrain;
      hit.distance = len2;
      results.push_back(hit);
    }
  }

  void debugDraw(DebugLineAdapter& debug, const Command& command, const std::vector<ShapeResult>& shapes, const std::vector<HitResult>& hits, const std::vector<Ray>& rays) {
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

  glm::vec2 computeImpulseType(const AreaForceStatEffect::Command::FlatImpulse& i, const HitResult& hit) {
    return hit.impulse * i.multiplier * static_cast<float>(hit.hitCount);
  }

  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("AreaForce Stat");
    auto ids = task.getIDResolver();
    auto query = task.query<
      const CommandRow
    >();
    DebugLineAdapter debug = TableAdapters::getDebugLines(task);
    using namespace Tags;
    auto shapesQuery = task.query<
      const FloatRow<GPos, X>, const FloatRow<GPos, Y>,
      const FloatRow<GRot, CosAngle>, const FloatRow<GRot, SinAngle>,
      const IsFragment
    >();
    auto resolver = task.getResolver<
      const IsImmobile,
      const FloatRow<GPos, X>, const FloatRow<GPos, Y>,
      FloatRow<GLinImpulse, X>, FloatRow<GLinImpulse, Y>,
      FloatRow<GAngImpulse, Angle>,
      const StableIDRow,
      const FragmentGoalFoundTableTag
    >();
    const TableID fragmentTable = builder.queryTables<FragmentGoalFoundRow>().matchingTableIDs[0];

    task.setCallback([ids, query, debug, shapesQuery, resolver, fragmentTable](AppTaskArgs& args) mutable {
      std::vector<ShapeResult> shapes;
      std::vector<HitResult> hits;
      std::vector<Ray> rays;
      CachedRow<const FloatRow<GPos, X>> posX;
      CachedRow<const FloatRow<GPos, Y>> posY;
      CachedRow<FloatRow<GLinImpulse, X>> impulseX;
      CachedRow<FloatRow<GLinImpulse, Y>> impulseY;
      CachedRow<FloatRow<GAngImpulse, Angle>> impulseA;
      CachedRow<const StableIDRow> stableRow;
      CachedRow<const FragmentGoalFoundTableTag> isCompletedFragment;
      Events::MovePublisher moveCompletedFragment{ &args };

      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands] = query.get(t);

        for(const Command& command : *commands) {
          shapes.clear();
          hits.clear();
          rays.clear();
          for(size_t s = 0; s < shapesQuery.size(); ++s) {
            auto&& [posXs, posYs, rotXs, rotYs, f] = shapesQuery.get(s);
            const UnpackedDatabaseElementID table = shapesQuery.matchingTableIDs[s];
            ShapesQuery shapeTable {
              table, posXs, posYs, rotXs, rotYs, nullptr, nullptr, nullptr, resolver->tryGetRow<const IsImmobile>(table)
            };
            std::visit([&](const auto& shape) {
              gatherResultsInShape(command, shape, shapeTable, shapes);
            }, command.shape);
          }

          std::visit([&](const auto& shape) {
            buildRaysForShape(command, shape, rays);
          }, command.shape);

          for(Ray& ray : rays) {
            ray.remainingPiercingTerrain = command.terrainPiercing;
            ray.remainingPiercingDynamic = command.dynamicPiercing;
          }

          const bool doDebugDraw = true;
          std::vector<ShapeResult> drawShapes;
          if(doDebugDraw) {
            drawShapes = shapes;
          }
          castRays(rays, shapes, hits);

          //TODO: read from SharedMassRow
          constexpr Math::Mass objMass = Math::computeFragmentMass();
          for(const HitResult& hit : hits) {
            const size_t id = hit.id.getElementIndex();
            //If fundamental information is missing, skip it
            if(!resolver->tryGetOrSwapAllRows(hit.id, posX, posY, stableRow)) {
              continue;
            }
            //If it has velocity, apply an impulse
            if(resolver->tryGetOrSwapAllRows(hit.id, impulseX, impulseY, impulseA)) {
              //Compute and apply impulse
              const glm::vec2 pos = TableAdapters::read(id, *posX, *posY);
              const glm::vec2 baseImpulse = std::visit([&hit](const auto& type) { return computeImpulseType(type, hit); }, command.impulseType);

              const Math::Impulse impulse = Math::computeImpulseAtPoint(pos, hit.hitPoint, baseImpulse, objMass);
              TableAdapters::add(id, impulse.linear, *impulseX, *impulseY);
              impulseA->at(id) += impulse.angular;

              DamageStatEffect::Builder dmgBuilder{ args };
              dmgBuilder.createStatEffects(1).setLifetime(StatEffect::INSTANT).setOwner(stableRow->at(id));
              dmgBuilder.setDamage(command.damage * static_cast<float>(hit.hitCount));
              continue;
            }
            //If it doesn't have velocity, is a completed fragment, and the command is supposed to dislodge it, then dislodge it
            else if(command.dislodgeFragments && resolver->tryGetOrSwapRow(isCompletedFragment, hit.id)) {
              moveCompletedFragment(stableRow->at(id), fragmentTable);
            }
          }

          debugDraw(debug, command, drawShapes, hits, rays);
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}