#include "Precompile.h"
#include "FragmentStateMachine.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "stat/FollowTargetByVelocityEffect.h"
#include "stat/LambdaStatEffect.h"

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
    static void onEnter(IAppBuilder&, const UnpackedDatabaseElementID&, size_t) {
    }

    static void onUpdate(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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

    static void onExit(IAppBuilder&, const UnpackedDatabaseElementID&, size_t) {
    }
  };

  template<>
  struct StateTraits<Wander> {
    static void onEnter(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("enter wander");
      auto spatialQuery = SpatialQuery::createCreator(task);
      auto query = task.query<const GlobalsRow, StateRow>(table);
      task.setCallback([query, bucket, spatialQuery](AppTaskArgs& args) mutable {
        auto&& [globals, state] = query.get(0);
        IRandom* random = ThreadLocalData::get(args).random;
        for(size_t i : globals->at().buckets[bucket].entering) {
          Wander& wander = std::get<Wander>(state->at(i).currentState);
          wander.spatialQuery = spatialQuery->createQuery({ SpatialQuery::AABB{} }, 2);
          wander.desiredDirection = random->nextDirection();
        }
      });

      builder.submitTask(std::move(task));
    }

    static constexpr float seekAhead = 2.0f;

    static SpatialQuery::AABB computeQueryVolume(const glm::vec2& pos, const glm::vec2& forward) {
      //Put query volume a bit out in front of the fragment
      const glm::vec2 queryPos = pos + forward*seekAhead;
      constexpr glm::vec2 half{ 0.5f, 0.5f };
      return { queryPos - half, queryPos + half };
    }

    static void onUpdate(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("wander update");
      auto query = task.query<
        const GlobalsRow,
        StateRow,
        const StableIDRow,
        FloatRow<Tags::GLinImpulse, Tags::X>,
        FloatRow<Tags::GLinImpulse, Tags::Y>,
        const FloatRow<Tags::GAngVel, Tags::Angle>,
        FloatRow<Tags::GAngImpulse, Tags::Angle>,
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
        auto&& [globals, state, stableRow, impulseX, impulseY, angVelRow, impulseA, rotX, rotY, posX, posY] = query.get(0);
        for(size_t si : globals->at().buckets[bucket].updating) {
          const size_t stableID = stableRow->at(si);
          const glm::vec2 pos = TableAdapters::read(si, *posX, *posY);

          Wander& wander = std::get<Wander>(state->at(si).currentState);
          spatialQueryR->begin(wander.spatialQuery);
          const auto nearby = tryGetFirstNearby(*spatialQueryR, ids->tryResolveRef(StableElementID::fromStableID(stableID)));

          if(nearby) {
            //TODO: this doesn't really do what it means to but works well enough visually
            constexpr float rotateForwardSpeed = 0.1f;
            if(auto key = bodyResolver->tryResolve(*nearby)) {
              //Figure out which direction to turn that will avoid collision with the other body assuming it keeps moving in its direction
              const glm::vec2 otherForward = bodyResolver->getLinearVelocity(*key);
              //This is going the same direction as the other, turn away from the direction it is going
              if(glm::dot(wander.desiredDirection, otherForward) > 0) {
                wander.desiredDirection = Math::rotate(wander.desiredDirection, Math::cross(wander.desiredDirection, otherForward) > 0.0f ? -rotateForwardSpeed : rotateForwardSpeed);
              }
              //This is going towards the other, turn away
              else {
                wander.desiredDirection = Math::rotate(wander.desiredDirection, Math::cross(wander.desiredDirection, -otherForward) > 0.0f ? -rotateForwardSpeed : rotateForwardSpeed);
              }
            }
          }

          constexpr float linearSpeed = 0.003f;
          constexpr float angularAvoidance = 0.05f;
          constexpr float angularAccelleration = 0.0025f;
          constexpr float minAngVel = 0.001f;
          constexpr float angularDamping = 0.001f;

          //Apply angular impulse towards desired move direction
          const float angVel = angVelRow->at(si);
          const glm::vec2 forward = TableAdapters::read(si, *rotX, *rotY);

          const float errorCos = glm::dot(forward, wander.desiredDirection);
          const float errorSin = Math::cross(forward, wander.desiredDirection);
          //The absolute value doesn't matter because it is clamped to angularAvoidance, make sure the direction is always towards the smallest angle between the vectors
          const float totalError = errorCos > 0.99f ? 0.0f : std::atan2f(errorSin, errorCos);
          //Determine the desired angular velocity that will fix an amount of error this frame
          const float desiredAngVel = glm::clamp(totalError, -angularAvoidance, angularAvoidance);
          //Approach the desired velocity in increments of the approach
          const float angularCorrection = Math::approachAbs(angVel, angularAccelleration, desiredAngVel) - angVel;

          if(std::abs(angularCorrection) > 0.001f) {
            impulseA->at(si) += angularCorrection;
          }
          if(std::abs(angVel) > minAngVel) {
            impulseA->at(si) += angVel * -angularDamping;
          }

          //Move forward regardless of if it's the desird direction
          const float speedMod = nearby ? 0.5f : 1.0f;
          TableAdapters::add(si, forward * linearSpeed * speedMod, *impulseX, *impulseY);

          spatialQueryW->refreshQuery(wander.spatialQuery, { computeQueryVolume(pos, wander.desiredDirection) }, 2);
        }
      });
      builder.submitTask(std::move(task));

      debugDraw(builder, table, bucket);
    }

    static void debugDraw(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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
          const auto myID = ids->tryResolveRef(StableElementID::fromStableID(stableRow->at(si)));

          //Draw the spatial query volume
          const auto volume = computeQueryVolume(myPos, wander.desiredDirection);
          DebugDrawer::drawAABB(debug, volume.min, volume.max, glm::vec3{ 0.5f });

          StableElementID temp = wander.spatialQuery;
          //Draw lines to each nearby spatial query result
          spatialQuery->begin(temp);
          while(const auto* n = spatialQuery->tryIterate()) {
            if(!isValidResult(*n, myID)) {
              continue;
            }
            if(auto resolved = ids->getRefResolver().tryUnpack(n->other)) {
              if(resolver->tryGetOrSwapAllRows(*resolved, resolvedX, resolvedY)) {
                const glm::vec2 resolvedPos = TableAdapters::read(resolved->getElementIndex(), *resolvedX, *resolvedY);
                DebugDrawer::drawLine(debug, myPos, resolvedPos, glm::vec3{ 1.0f });
              }
            }
          }

          //Draw line in desired direction
          DebugDrawer::drawVector(debug, myPos, wander.desiredDirection, { 0, 1, 1 });
        }
      });

      builder.submitTask(std::move(task));
    }

    //Cleanup of the query isn't needed because it'll expire after its lifetime
    static void onExit(IAppBuilder&, const UnpackedDatabaseElementID&, size_t) {
    }
  };

  template<>
  struct StateTraits<Stunned> {
    static void onEnter(IAppBuilder&, const UnpackedDatabaseElementID&, size_t) {
    }
    static void onUpdate(IAppBuilder&, const UnpackedDatabaseElementID&, size_t) {
    }
    static void onExit(IAppBuilder&, const UnpackedDatabaseElementID&, size_t) {
    }
  };

  template<>
  struct StateTraits<SeekHome> {
    static void onEnter(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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

    static void onUpdate(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("enter SeekHome");
      auto query = task.query<
        const GlobalsRow,
        StateRow,
        const Tags::GPosXRow,
        const Tags::GPosYRow,
        const Tags::GLinVelXRow,
        const Tags::GLinVelYRow,
        const Tags::GAngVelRow,
        const Tags::FragmentGoalXRow,
        const Tags::FragmentGoalYRow,
        const ConstraintSolver::SharedMassRow,
        Tags::GLinImpulseXRow,
        Tags::GLinImpulseYRow,
        Tags::GAngImpulseRow
      >(table);

      PGS::SolverStorage solver;
      //Zero mass body and the one we'll solve for. Must have 2 since solver assumes pairs
      solver.resize(2, 2);
      solver.setMass(1, 0, 0);
      solver.setVelocity(1, glm::vec2{ 0 }, 0);
      task.setCallback([query, bucket, solver](AppTaskArgs&) mutable {
        auto&& [globals, state, posX, posY, vx, vy, va, goalX, goalY, massRow, ix, iy, ia] = query.get(0);
        solver.setMass(0, massRow->at().inverseMass, massRow->at().inverseInertia);

        for(size_t i : globals->at().buckets[bucket].updating) {
          SeekHome& seek = std::get<SeekHome>(state->at(i).currentState);
          if(seek.timer.tick()) {
            SM::setState(state->at(i), ExitSeekHome{});
            continue;
          }

          const glm::vec2 linVel = TableAdapters::read(i, *vx, *vy);
          const float angVel = va->at(i);
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
          targetVelocity;
          constexpr glm::vec2 z{ 0.0f };
          const glm::vec2 goalDir = toGoal / distance;
          //No warm start
          solver.setWarmStart(0, 0);
          solver.setWarmStart(1, 0);
          solver.setJacobian(0, 0, 1, goalDir, 0.0f, z, 0);
          solver.setJacobian(1, 0, 1, Geo::orthogonal(goalDir), 0, z, 0);
          //Try to hit target velocity but only ever push towards it
          solver.setBias(0, targetVelocity);
          solver.setLambdaBounds(0, 0.0f, maxForce);
          //solver.setLambdaBounds(0, 0, PGS::SolverStorage::UNLIMITED_MAX);
          solver.setLambdaBounds(1, -orbitPrevention, orbitPrevention);
          solver.setVelocity(0, linVel, angVel);
          solver.premultiply();

          auto context = solver.createContext();
          PGS::solvePGS(context);

          auto body = context.velocity.getBody(0);
          const glm::vec2 linearImpulse = body.linear - linVel;
          TableAdapters::add(i, linearImpulse, *ix, *iy);
        }
      });
      builder.submitTask(std::move(task));

      debugDraw(builder, table, bucket);
    }

    //Reset this in case the state wasn't exited to ExitSeekHome, as would happen if the fragment found its goal
    //Another possibility would be having the goal find set the mask
    static void onExit(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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

    static void debugDraw(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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
    static void onEnter(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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

    static void onUpdate(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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
          spatialQueryR->begin(key->stable);
          const auto self = ids->tryResolveRef(StableElementID::fromStableID(stableRow->at(i)));
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

    static void onExit(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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

    static void debugDraw(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
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