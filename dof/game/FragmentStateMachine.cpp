#include "Precompile.h"
#include "FragmentStateMachine.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "stat/FollowTargetByVelocityEffect.h"

#include "glm/glm.hpp"
#include "glm/gtx/norm.hpp"
#include "AppBuilder.h"

#include "DebugDrawer.h"
#include "ThreadLocals.h"
#include "Random.h"
#include "SpatialQueries.h"
#include "PhysicsSimulation.h"
#include "GameMath.h"
#include "PGSSolver.h"
#include "generics/Timer.h"
#include "ConstraintSolver.h"
#include "Geometric.h"
#include "Constraints.h"
#include "Clip.h"

namespace SM {
  using namespace FragmentStateMachine;
  //Skip results hitting the shape itself and results not aligned on the Z axis
  bool isValidResult(const SpatialQuery::Result& result, const ElementRef& self) {
    return
      result.other != self &&
      std::holds_alternative<SpatialQuery::ContactXY>(result.contact);
  }

  std::optional<ElementRef> tryGetFirstNearby(SpatialQuery::IReader& reader, const ElementRef& self) {
    const SpatialQuery::Result* result = reader.tryIterate();
    while(result && !isValidResult(*result, self)) {
      result = reader.tryIterate();
    }
    return result ? std::make_optional(result->other) : std::nullopt;
  }

  const bool* getShouldDrawAI(RuntimeDatabaseTaskBuilder& task) {
    return &TableAdapters::getGameConfig(task)->fragment.drawAI;
  }

  template<>
  struct StateTraits<Idle> {
    static void onEnter(IAppBuilder&, const TableID&, size_t) {
    }

    static void onUpdate(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("Idle update");
      using namespace Tags;
      auto query = task.query<
        const GlobalsRow,
        const FloatRow<GLinVel, X>, const FloatRow<GLinVel, Y>,
        StateRow
      >(table);

      task.setCallback([query, bucket](AppTaskArgs&) mutable {
        auto&& [globals, vx, vy, state] = query.get(0);
        for(size_t i : globals->at().buckets[bucket].updating) {
          constexpr float activationThreshold = 0.0001f;
          const float speed2 = glm::length2(TableAdapters::read(i, *vx, *vy));
          if(speed2 > activationThreshold) {
            state->at(i).desiredState = Wander{};
          }
        }
      });

      builder.submitTask(std::move(task));
    }

    static void onExit(IAppBuilder&, const TableID&, size_t) {
    }
  };

  template<>
  struct StateTraits<Wander> {
    static void onEnter(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("enter wander");
      auto spatialQuery = SpatialQuery::createCreator(task);
      auto query = task.query<const GlobalsRow, StateRow>(table);
      task.setCallback([query, bucket, spatialQuery](AppTaskArgs& args) mutable {
        auto&& [globals, state] = query.get(0);
        IRandom* random = args.getRandom();
        for(size_t i : globals->at().buckets[bucket].entering) {
          Wander& wander = std::get<Wander>(state->at(i).currentState);
          wander.spatialQuery = spatialQuery->createQuery({ SpatialQuery::AABB{} }, 2);
          wander.desiredDirection = random->nextDirection();
        }
      });

      builder.submitTask(std::move(task));
    }

    static constexpr float seekAhead = 2.0f;

    static SpatialQuery::AABB computeQueryVolume(const glm::vec2& pos, [[maybe_unused]] const glm::vec2& forward) {
      //Put query volume a bit out in front of the fragment
      const glm::vec2 queryPos = pos;// + forward*seekAhead;
      constexpr glm::vec2 half{ seekAhead * 0.5f };
      return { queryPos - half, queryPos + half };
    }

    //Gather a vague idea of the obstructions through the contact points. This will vary in accuracy depending on what type of collision it is
    static void buildObstacles(
      std::vector<Clip::StartAndDir>& buffer,
      SpatialQuery::IReader& spatialQueryR,
      const Wander& wander,
      const ElementRef& self,
      const glm::vec2& queryPos) {
      spatialQueryR.begin(wander.spatialQuery);
      buffer.clear();
      //Gather a vague idea of the obstructions through the contact points. This will vary in accuracy depending on what type of collision it is
      while(const SpatialQuery::Result* q = spatialQueryR.tryIterate()) {
        if(auto contact = std::get_if<SpatialQuery::ContactXY>(&q->contact); q->other != self && contact && contact->points.size() >= 2) {
          Clip::StartAndDir obstacle{ .start{ contact->points[0].point } };
          obstacle.dir = contact->points[1].point - obstacle.start;
          obstacle.start += queryPos;
          buffer.push_back(obstacle);
        }
      };
    }

    static bool isObstructedDirection(const Clip::StartAndDir& myDir, const std::vector<Clip::StartAndDir>& obstacles) {
      for(const Clip::StartAndDir& obstacle : obstacles) {
        const Clip::LineLineIntersectTimes intersect = Clip::getIntersectTimes(myDir, obstacle);
        //If it's parallel or backwards, not obstructed
        if(!intersect || *intersect.tA < 0.f) {
          continue;
        }

        const float clampedOnObstacle = glm::clamp(*intersect.tB, 0.f, 1.f);
        const glm::vec2 closestOnB = obstacle.start + obstacle.dir*clampedOnObstacle;
        //A is looking forward infinitely so no clamping is needed
        const glm::vec2 toB = closestOnB - myDir.start;
        //Projection of closestOnB onto the forward direction.
        //If they were already intersecting this would be the intersect, otherwise it's the closest on A
        const glm::vec2 closestOnA = closestOnB - (myDir.dir * glm::dot(myDir.dir, toB));
        constexpr float requiredClearance2 = Geo::squared(1.5f);
        //See if there is enough space between the forward line and the obstacle face
        if(glm::distance2(closestOnA, closestOnB) < requiredClearance2) {
          return true;
        }
      }
      return false;
    }

    static std::optional<glm::vec2> findUnobstructedDirection(const std::vector<Clip::StartAndDir>& obstacles, const glm::vec2& pos, const glm::vec2& startDir) {
      constexpr int maxAttempts = 5;
      constexpr float angleIncrement = Constants::PI / static_cast<float>(maxAttempts);
      //Search alternating angles rotated away from the current wander direction for the first that is unobstructed
      Clip::StartAndDir myDir{ .start{ pos } };
      for(float a = 0; a < Constants::PI; a += angleIncrement) {
        const glm::vec2 r = Geo::directionFromAngle(angleIncrement * static_cast<float>(a));
        for(const auto rotation : { r, Geo::transposeRot(r) }) {
          myDir.dir = Geo::rotate(rotation, startDir);
          if(!isObstructedDirection(myDir, obstacles)) {
            return myDir.dir;
          }
        }
      }
      //All options exhaused, nothing found
      return {};
    }

    static void onUpdate(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("wander update");
      auto query = task.query<
        const GlobalsRow,
        StateRow,
        const StableIDRow,
        Constraints::JointRow,
        const FloatRow<Tags::GRot, Tags::CosAngle>,
        const FloatRow<Tags::GRot, Tags::SinAngle>,
        const FloatRow<Tags::GPos, Tags::X>,
        const FloatRow<Tags::GPos, Tags::Y>
      >(table);
      auto spatialQueryR = SpatialQuery::createReader(task);
      auto spatialQueryW = SpatialQuery::createWriter(task);
      auto bodyResolver = PhysicsSimulation::createPhysicsBodyResolver(task);
      auto ids = task.getIDResolver();

      task.setCallback([ids, query, spatialQueryR, spatialQueryW, bucket, bodyResolver](AppTaskArgs&) mutable {
        auto&& [globals, state, stableRow, joints, rotX, rotY, posX, posY] = query.get(0);
        for(size_t si : globals->at().buckets[bucket].updating) {
          const ElementRef stableID = stableRow->at(si);
          const glm::vec2 pos = TableAdapters::read(si, *posX, *posY);

          Wander& wander = std::get<Wander>(state->at(si).currentState);
          spatialQueryR->begin(wander.spatialQuery);

          std::vector<Clip::StartAndDir> obstacles;
          buildObstacles(obstacles, *spatialQueryR, wander, stableID, pos);
          const std::optional<glm::vec2> unobstructedDir = findUnobstructedDirection(obstacles, pos, wander.desiredDirection);

          if(unobstructedDir) {
            wander.desiredDirection = *unobstructedDir;
          }

          constexpr float linearForce = 0.003f;
          constexpr float linearSpeed = 0.1f;
          constexpr float orthogonalForce = 0.03f;

          //const float speedMod = nearby ? 0.5f : 1.0f;
          Constraints::PinMotorJoint joint;
          joint.force = linearForce;// * speedMod;
          joint.orthogonalForce = orthogonalForce;
          joint.localCenterToPinA = glm::vec2{ 1.f, 0.f };
          joint.targetVelocity = wander.desiredDirection * linearSpeed;

          joints->at(si) = { joint };

          spatialQueryW->refreshQuery(wander.spatialQuery, { computeQueryVolume(pos, wander.desiredDirection) }, 2);
        }
      });
      builder.submitTask(std::move(task));

      debugDraw(builder, table, bucket);
    }

    static void debugDraw(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("wander debug");
      auto query = task.query<
        const GlobalsRow,
        const StateRow,
        const StableIDRow,
        const FloatRow<Tags::GPos, Tags::X>,
        const FloatRow<Tags::GPos, Tags::Y>,
        const FloatRow<Tags::GRot, Tags::CosAngle>,
        const FloatRow<Tags::GRot, Tags::SinAngle>
      >(table);
      auto resolver = task.getResolver<
        FloatRow<Tags::GPos, Tags::X>,
        FloatRow<Tags::GPos, Tags::Y>
      >();
      auto ids = task.getIDResolver();
      auto spatialQuery = SpatialQuery::createReader(task);
      auto debug = TableAdapters::getDebugLines(task);
      const bool* shouldDrawAI = getShouldDrawAI(task);

      task.setCallback([debug, query, spatialQuery, bucket, shouldDrawAI, resolver, ids](AppTaskArgs&) mutable {
        if(!*shouldDrawAI) {
          return;
        }
        auto&& [globals, state, stableRow, posX, posY, rotX, rotY] = query.get(0);
        CachedRow<FloatRow<Tags::GPos, Tags::X>> resolvedX;
        CachedRow<FloatRow<Tags::GPos, Tags::Y>> resolvedY;
        for(size_t si : globals->at().buckets[bucket].updating) {
          const Wander& wander = std::get<Wander>(state->at(si).currentState);
          const glm::vec2 myPos = TableAdapters::read(si, *posX, *posY);
          const glm::vec2 myForward = TableAdapters::read(si, *rotX, *rotY);
          const ElementRef myID = stableRow->at(si);

          //Draw the spatial query volume
          const auto volume = computeQueryVolume(myPos, wander.desiredDirection);
          DebugDrawer::drawAABB(debug, volume.min, volume.max, glm::vec3{ 0.5f });

          std::vector<Clip::StartAndDir> obstacles;
          buildObstacles(obstacles, *spatialQuery, wander, myID, myPos);
          for(const Clip::StartAndDir& o : obstacles) {
            DebugDrawer::drawLine(debug, o.start, o.start + o.dir, glm::vec3{ 0.5f, 0.1f, 1.f });
          }

          //Draw line in desired direction
          DebugDrawer::drawVector(debug, myPos, wander.desiredDirection, { 0, 1, 1 });
        }
      });

      builder.submitTask(std::move(task));
    }

    //Cleanup of the query isn't needed because it'll expire after its lifetime
    static void onExit(IAppBuilder&, const TableID&, size_t) {
    }
  };

  template<>
  struct StateTraits<Stunned> {
    static void onEnter(IAppBuilder&, const TableID&, size_t) {
    }
    static void onUpdate(IAppBuilder&, const TableID&, size_t) {
    }
    static void onExit(IAppBuilder&, const TableID&, size_t) {
    }
  };

  template<>
  struct StateTraits<SeekHome> {
    static void onEnter(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("enter SeekHome");
      auto query = task.query<
        const GlobalsRow,
        StateRow,
        Narrowphase::CollisionMaskRow,
        Tint
      >(table);

      task.setCallback([query, bucket](AppTaskArgs&) mutable {
        auto&& [globals, state, collisionMask, tint] = query.get(0);

        for(size_t i : globals->at().buckets[bucket].entering) {
          //Set color
          tint->at(i).r = 1.0f;
          collisionMask->at(i) = Narrowphase::CollisionMask(0);

          SeekHome& seek = std::get<SeekHome>(state->at(i).currentState);
          seek.timer.start(100);
        }
      });

      builder.submitTask(std::move(task));
    }

    static void onUpdate(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("enter SeekHome");
      auto query = task.query<
        const GlobalsRow,
        StateRow,
        const Tags::GPosXRow,
        const Tags::GPosYRow,
        Constraints::JointRow,
        Constraints::CustomConstraintRow,
        const Tags::FragmentGoalXRow,
        const Tags::FragmentGoalYRow
      >(table);

      task.setCallback([query, bucket](AppTaskArgs&) mutable {
        auto&& [globals, state, posX, posY, joints, customs, goalX, goalY] = query.get(0);

        for(size_t i : globals->at().buckets[bucket].updating) {
          SeekHome& seek = std::get<SeekHome>(state->at(i).currentState);
          if(seek.timer.tick()) {
            SM::setState(state->at(i), ExitSeekHome{});
            continue;
          }

          const glm::vec2 myPos = TableAdapters::read(i, *posX, *posY);
          const glm::vec2 goal = TableAdapters::read(i, *goalX, *goalY);
          const glm::vec2 toGoal = goal - myPos;
          const float distance = glm::length(toGoal);
          if(distance < 0.001f) {
            continue;
          }
          constexpr float maxForce = 0.01f;
          constexpr float targetVelocity = 1.1f;
          constexpr float orbitPrevention = 0.01f;
          const glm::vec2 goalDir = toGoal / distance;
          joints->at(i) = { Constraints::CustomJoint{} };
          Constraints::Constraint3DOF& custom = customs->at(i);
          //Clear to be safe against bleeding values from an unrelated state
          custom = {};

          //Push towards goal and apply a small corrective force orthogonally to push a bit against orbiting around the target
          custom.sideA[0].linear = goalDir;
          custom.sideA[1].linear = Geo::orthogonal(goalDir);
          custom.common[0].bias = targetVelocity;
          custom.common[0].lambdaMax = maxForce;
          custom.common[1].lambdaMin = -orbitPrevention;
          custom.common[1].lambdaMax = orbitPrevention;
          custom.setEnd(2);
        }
      });
      builder.submitTask(std::move(task));

      debugDraw(builder, table, bucket);
    }

    //Reset this in case the state wasn't exited to ExitSeekHome, as would happen if the fragment found its goal
    //Another possibility would be having the goal find set the mask
    static void onExit(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("Exit SeekHome");

      auto query = task.query<
        const GlobalsRow,
        const StateRow,
        Narrowphase::CollisionMaskRow
      >(table);

      task.setCallback([query, bucket](AppTaskArgs&) mutable {
        auto&& [globals, state, collisionMask] = query.get(0);
        for(size_t i : globals->at().buckets[bucket].exiting) {
          collisionMask->at(i) = Narrowphase::CollisionMask(~0);
        }
      });
      builder.submitTask(std::move(task));
    }

    static void debugDraw(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("seekhome debug");
      auto query = task.query<
        const GlobalsRow,
        const StateRow,
        const Tags::GPosXRow,
        const Tags::GPosYRow,
        const Tags::FragmentGoalXRow,
        const Tags::FragmentGoalYRow
      >(table);
      auto debug = TableAdapters::getDebugLines(task);
      const bool* shouldDrawAI = getShouldDrawAI(task);

      task.setCallback([debug, query, bucket, shouldDrawAI](AppTaskArgs&) mutable {
        if(!*shouldDrawAI) {
          return;
        }
        auto&& [globals, state, posX, posY, goalX, goalY] = query.get(0);
        for(size_t si : globals->at().buckets[bucket].updating) {
          const glm::vec2 myPos = TableAdapters::read(si, *posX, *posY);
          const glm::vec2 goal = TableAdapters::read(si, *goalX, *goalY);
          DebugDrawer::drawLine(debug, myPos, goal, glm::vec3{ 0, 1, 0 });
        }
      });
      builder.submitTask(std::move(task));
    }
  };

  template<>
  struct StateTraits<ExitSeekHome> {
    static void onEnter(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("enter ExitSeekHome");
      auto query = task.query<
        const GlobalsRow,
        const Tags::GLinVelXRow,
        const Tags::GLinVelYRow,
        Narrowphase::CollisionMaskRow,
        StateRow
      >(table);
      auto spatialQuery = SpatialQuery::createCreator(task);

      task.setCallback([query, bucket, spatialQuery](AppTaskArgs&) mutable {
        auto&& [globals, vx, vy, collisionMask, state] = query.get(0);

        for(size_t i : globals->at().buckets[bucket].entering) {
          collisionMask->at(i) = Narrowphase::CollisionMask(0);
          ExitSeekHome& seek = std::get<ExitSeekHome>(state->at(i).currentState);
          seek.spatialQuery = spatialQuery->createQuery({ SpatialQuery::AABB{ glm::vec2{ 0 }, glm::vec2{ 0 } } }, 2);
          const glm::vec2 v = TableAdapters::read(i, *vx, *vy);
          const float length = glm::length(v);
          constexpr float speed = 0.01f;
          //Try to keep going in the direction it was already going. If it wasn't moving pick an arbitrary one
          seek.direction = length > 0.001f ? v * (speed/length) : glm::vec2{ speed, 0 };
        }
      });

      builder.submitTask(std::move(task));
    }

    static SpatialQuery::AABB getQueryAABB(const glm::vec2& myPos) {
      const glm::vec2 halfSize{ 1.0f, 1.0f };
      return SpatialQuery::AABB{ myPos - halfSize, myPos + halfSize };
    }

    static void onUpdate(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("update ExitSeekHome");
      auto query = task.query<
        const GlobalsRow,
        StateRow,
        const StableIDRow,
        const Tags::GPosXRow,
        const Tags::GPosYRow,
        Tags::GLinImpulseXRow,
        Tags::GLinImpulseYRow
      >(table);
      auto resolver = task.getResolver<const StateRow, const SpatialQueriesTableTag>();
      auto ids = task.getIDResolver();

      auto spatialQueryW = SpatialQuery::createWriter(task);
      auto spatialQueryR = SpatialQuery::createReader(task);
      task.setCallback([query, bucket, spatialQueryR, spatialQueryW, resolver, ids](AppTaskArgs&) mutable {
        auto&& [globals, state, stableRow, posX, posY, ix, iy] = query.get(0);
        CachedRow<const StateRow> stateLookup;
        CachedRow<const SpatialQueriesTableTag> spatialQueryLookup;

        for(size_t i : globals->at().buckets[bucket].updating) {
          ExitSeekHome& seek = std::get<ExitSeekHome>(state->at(i).currentState);

          //Keep going in some direction so this doesn't get stuck forever
          TableAdapters::add(i, seek.direction, *ix, *iy);

          const glm::vec2 myPos = TableAdapters::read(i, *posX, *posY);
          auto key = spatialQueryW->getKey(seek.spatialQuery);
          if(!key) {
            //Shouldn't happen
            continue;
          }

          //Write the new query position, this doesn't affect  reading the results below
          SpatialQuery::Query newQuery{ getQueryAABB(myPos) };
          spatialQueryW->swapQuery(*key, newQuery, 2);
          //See if the contained query had the initial zero value or a real value. If it was initial, wait until next frame
          if(auto bb = std::get_if<SpatialQuery::AABB>(&newQuery.shape)) {
            if(bb->min == glm::vec2{ 0 }) {
              continue;
            }
          }

          //If there is nothing nearby it's safe to exit the state
          spatialQueryR->begin(seek.spatialQuery);
          const ElementRef self = stableRow->at(i);
          bool foundObstacle = false;
          while(auto* r = spatialQueryR->tryIterate()) {
            if(r->other == self) {
              continue;
            }
            //Ignore others that are also in this state
            //TODO: this probably makes mores sense to do with collision layers
            if(auto resolved = ids->getRefResolver().tryUnpack(r->other)) {
              if(const auto* s = resolver->tryGetOrSwapRowElement(stateLookup, *resolved)) {
                 if(std::get_if<SeekHome>(&s->currentState) || std::get_if<ExitSeekHome>(&s->currentState)) {
                    continue;
                 }
              }
              //Ignoer other spatial queries
              if(resolver->tryGetOrSwapRow(spatialQueryLookup, *resolved)) {
                continue;
              }
              foundObstacle = true;
              break;
            }
          }
          if(!foundObstacle) {
            SM::setState(state->at(i), Wander{});
          }
        }
      });
      builder.submitTask(std::move(task));

      debugDraw(builder, table, bucket);
    }

    static void onExit(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("Exit ExitSeekHome");

      auto query = task.query<
        const GlobalsRow,
        const StateRow,
        Narrowphase::CollisionMaskRow,
        Tint
      >(table);

      task.setCallback([query, bucket](AppTaskArgs&) mutable {
        auto&& [globals, state, collisionMask, tint] = query.get(0);
        for(size_t i : globals->at().buckets[bucket].exiting) {
          //Reset color
          tint->at(i).r = 0.0f;
          collisionMask->at(i) = Narrowphase::CollisionMask(~0);
        }
      });
      builder.submitTask(std::move(task));
    }

    static void debugDraw(IAppBuilder& builder, const TableID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("seekhome debug");
      auto query = task.query<
        const GlobalsRow,
        const Tags::GPosXRow,
        const Tags::GPosYRow
      >(table);
      auto debug = TableAdapters::getDebugLines(task);
      const bool* shouldDrawAI = getShouldDrawAI(task);

      task.setCallback([debug, query, bucket, shouldDrawAI](AppTaskArgs&) mutable {
        if(!*shouldDrawAI) {
          return;
        }
        auto&& [globals, posX, posY] = query.get(0);
        for(size_t si : globals->at().buckets[bucket].updating) {
          const auto bb = getQueryAABB(TableAdapters::read(si, *posX, *posY));
          DebugDrawer::drawAABB(debug, bb.min, bb.max, glm::vec3{ 0.5f, 0.5f, 0.5f });
        }
      });
      builder.submitTask(std::move(task));
    }
  };

  struct GetStateName {
    static constexpr std::string_view MY_NAMESPACE = "FragmentStateMachine::";
    template<class T>
    std::string_view operator()(const T&) {
      return TypeName<std::decay_t<T>>::get().substr(MY_NAMESPACE.size());
    }
  };

  void printStateNames(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("print state names");
    auto debug = TableAdapters::getDebugLines(task);
    auto cfg = TableAdapters::getGameConfig(task);
    auto query = task.query<const FragmentStateMachine::StateRow,
      const FloatRow<Tags::GPos, Tags::X>,
      const FloatRow<Tags::GPos, Tags::Y>
    >();
    task.setCallback([debug, cfg, query](AppTaskArgs&) mutable {
      if(!cfg->fragment.drawAI) {
        return;
      }
      for(size_t t = 0; t < query.size(); ++t) {
        auto [state, posX, posY] = query.get(t);
        for(size_t i = 0; i < state->size(); ++i) {
          constexpr glm::vec2 offset{ -1.0f, -1.0f };
          const glm::vec2 pos = TableAdapters::read(i, *posX, *posY);
          DebugDrawer::drawText(debug, pos + offset, std::string{ std::visit(GetStateName{}, state->at(i).currentState) });
        }
      }
    });
    builder.submitTask(std::move(task));
  }
}

namespace FragmentStateMachine {
  void setState(FragmentState& current, FragmentStateMachineT::Variant&& desired) {
    SM::setState(current, std::move(desired));
  }

  void update(IAppBuilder& builder) {
    SM::update<FragmentStateMachineT>(builder);
    SM::printStateNames(builder);
  }

  void preProcessEvents(IAppBuilder& builder) {
    SM::preProcessEvents<FragmentStateMachineT>(builder);
  }
}