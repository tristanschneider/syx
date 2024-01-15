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

namespace SM {
  using namespace FragmentStateMachine;
  std::optional<StableElementID> tryGetFirstNearby(SpatialQuery::IReader& reader, const StableElementID& self) {
    const SpatialQuery::Result* result = reader.tryIterate();
    while(result && result->other == self) {
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

      task.setCallback([query, spatialQueryR, spatialQueryW, bucket, bodyResolver](AppTaskArgs&) mutable {
        auto&& [globals, state, stableRow, impulseX, impulseY, angVelRow, impulseA, rotX, rotY, posX, posY] = query.get(0);
        for(size_t si : globals->at().buckets[bucket].updating) {
          const size_t stableID = stableRow->at(si);
          const glm::vec2 pos = TableAdapters::read(si, *posX, *posY);

          Wander& wander = std::get<Wander>(state->at(si).currentState);
          spatialQueryR->begin(wander.spatialQuery);
          const auto nearby = tryGetFirstNearby(*spatialQueryR, StableElementID::fromStableID(stableID));

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
          const size_t myID = stableRow->at(si);

          //Draw the spatial query volume
          const auto volume = computeQueryVolume(myPos, wander.desiredDirection);
          DebugDrawer::drawAABB(debug, volume.min, volume.max, glm::vec3{ 0.5f });

          StableElementID temp = wander.spatialQuery;
          //Draw lines to each nearby spatial query result
          spatialQuery->begin(temp);
          while(const auto* n = spatialQuery->tryIterate()) {
            if(n->other.mStableID == myID) {
              continue;
            }
            if(auto resolved = ids->tryResolveAndUnpack(n->other)) {
              if(resolver->tryGetOrSwapAllRows(resolved->unpacked, resolvedX, resolvedY)) {
                const glm::vec2 resolvedPos = TableAdapters::read(resolved->unpacked.getElementIndex(), *resolvedX, *resolvedY);
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
        const StableIDRow,
        Tint
      >(table);
      const UnpackedDatabaseElementID targetTable = builder.queryTables<TargetTableTag>().matchingTableIDs[0];
      auto targetModifier = task.getModifierForTable(targetTable);
      auto targets = task.query<
        const StableIDRow
      >(targetTable);

      task.setCallback([query, bucket, targetModifier, targets](AppTaskArgs& args) mutable {
        auto&& [globals, state, stableRow, tint] = query.get(0);

        for(size_t i : globals->at().buckets[bucket].entering) {
          //Set color
          tint->at(i).r = 1.0f;

          SeekHome& seek = std::get<SeekHome>(state->at(i).currentState);

          //Create target for follow behavior
          const size_t& stableId = stableRow->at(i);
          const size_t newTarget = targetModifier->addElements(1);
          StableElementID stableTarget = StableElementID::fromStableRow(newTarget, targets.get<0>(0));
          seek.target = stableTarget;

          const size_t followTime = 100;
          const float springConstant = 0.001f;

          //Create the follow effect and point it at the target
          auto followVelocity = TableAdapters::getFollowTargetByVelocityEffects(args);
          const size_t followEffect = TableAdapters::addStatEffectsSharedLifetime(followVelocity.base, followTime, &stableId, 1);
          followVelocity.command->at(followEffect).mode = FollowTargetByVelocityStatEffect::SpringFollow{ springConstant };
          followVelocity.base.target->at(followEffect) = stableTarget;
          //Enqueue the state transition back to wander
          followVelocity.base.continuations->at(followEffect).onComplete.push_back([stableId](StatEffect::Continuation::Args& a) {
            RuntimeDatabaseTaskBuilder task{ a.db };
            auto ids = task.getIDResolver();
            auto resolver = task.getResolver<StateRow>();

            if(auto resolved = ids->tryResolveStableID(StableElementID::fromStableID(stableId))) {
              const auto unpacked = ids->uncheckedUnpack(*resolved);
              if(StateRow* row = resolver->tryGetRow<StateRow>(unpacked)) {
                SM::setState(row->at(unpacked.getElementIndex()), Wander{});
              }
            }

            task.discard();
          });
        }
      });

      builder.submitTask(std::move(task));
    }

    static void onUpdate(IAppBuilder&, const UnpackedDatabaseElementID&, size_t) {
    }

    static void onExit(IAppBuilder& builder, const UnpackedDatabaseElementID& table, size_t bucket) {
      auto task = builder.createTask();
      task.setName("Exit SeekHome");

      auto query = task.query<
        const GlobalsRow,
        const StateRow,
        Tint
      >(table);

      task.setCallback([query, bucket](AppTaskArgs& args) mutable {
        auto&& [globals, state, tint] = query.get(0);
        for(size_t i : globals->at().buckets[bucket].exiting) {
          //Reset color
          tint->at(i).r = 0.0f;

          const SeekHome& seek = std::get<SeekHome>(state->at(i).previousState);

          StableElementID toRemove = seek.target;
          Events::onRemovedElement(toRemove, args);
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