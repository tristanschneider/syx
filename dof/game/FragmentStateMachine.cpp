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

namespace SM {
  using namespace FragmentStateMachine;
  std::optional<StableElementID> tryGetFirstNearby(const SpatialQuery::Result& results, const StableElementID& self) {
    auto it = std::find_if(results.nearbyObjects.begin(), results.nearbyObjects.end(), [&](const StableElementID& e) { return e.mStableID != self.mStableID; });
    return it != results.nearbyObjects.end() ? std::make_optional(*it) : std::nullopt;
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
      task.setCallback([query, bucket, spatialQuery](AppTaskArgs&) mutable {
        auto&& [globals, state] = query.get(0);
        for(size_t i : globals->at().buckets[bucket].entering) {
          std::get<Wander>(state->at(i).currentState).spatialQuery = spatialQuery->createQuery({ SpatialQuery::AABB{} }, 2);
        }
      });

      builder.submitTask(std::move(task));
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

      task.setCallback([query, spatialQueryR, spatialQueryW, bucket](AppTaskArgs&) mutable {
        auto&& [globals, state, stableRow, impulseX, impulseY, angVelRow, impulseA, rotX, rotY, posX, posY] = query.get(0);
        for(size_t si : globals->at().buckets[bucket].updating) {
          const size_t stableID = stableRow->at(si);

          Wander& wander = std::get<Wander>(state->at(si).currentState);
          const SpatialQuery::Result* results = spatialQueryR->tryGetResult(wander.spatialQuery);
          if(!results) {
            continue;
          }

          constexpr float linearSpeed = 0.003f;
          constexpr float angularAvoidance = 0.01f;
          constexpr float maxAngVel = 0.03f;
          constexpr float minAngVel = 0.001f;
          constexpr float angularDamping = 0.001f;
          constexpr float seekAhead = 2.0f;

          //Don't tinker with direction if already spinning out of control
          const float angVel = angVelRow->at(si);
          const float angSpeed =  std::abs(angVel);
          if(angSpeed < maxAngVel) {
            //If something is in the way, turn to the right
            if(tryGetFirstNearby(*results, StableElementID::fromStableID(stableID))) {
              impulseA->at(si) += angularAvoidance;
            }
          }
          if(angSpeed > minAngVel) {
            impulseA->at(si) += angVel * -angularDamping;
          }

          const glm::vec2 forward = TableAdapters::read(si, *rotX, *rotY);
          TableAdapters::add(si, forward * linearSpeed, *impulseX, *impulseY);

          //Put query volume a bit out in front of the fragment with size of 0.5
          const glm::vec2 pos = TableAdapters::read(si, *posX, *posY);
          const glm::vec2 queryPos = pos + forward*seekAhead;
          constexpr glm::vec2 half{ 0.25f, 0.25f };
          SpatialQuery::AABB bb{ queryPos - half, queryPos + half };
          spatialQueryW->refreshQuery(wander.spatialQuery, { bb }, 2);
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