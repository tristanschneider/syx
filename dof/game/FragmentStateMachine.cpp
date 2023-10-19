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

namespace FragmentStateMachine {
  std::optional<StableElementID> tryGetFirstNearby(const SpatialQuery::Result& results, const StableElementID& self) {
    auto it = std::find_if(results.nearbyObjects.begin(), results.nearbyObjects.end(), [&](const StableElementID& e) { return e.mStableID != self.mStableID; });
    return it != results.nearbyObjects.end() ? std::make_optional(*it) : std::nullopt;
  }

  void setState(FragmentState& current, FragmentState::Variant&& desired) {
    std::lock_guard<std::mutex> lock{ current.desiredStateMutex };
    current.desiredState = std::move(desired);
  }

  template<class StateT>
  std::optional<typename StateT::Variant> tryTakeDesiredState(StateT& current) {
    std::lock_guard<std::mutex> lock{ current.desiredStateMutex };
    return current.currentState.index() != current.desiredState.index() ? std::make_optional(current.desiredState) : std::nullopt;
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
                setState(row->at(unpacked.getElementIndex()), Wander{});
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

          const SeekHome& seek = std::get<SeekHome>(state->at(i).currentState);

          StableElementID toRemove = seek.target;
          auto lambda = TableAdapters::getLambdaEffects(args);
          size_t effect = TableAdapters::addStatEffectsSharedLifetime(lambda.base, StatEffect::INSTANT, &toRemove.mStableID, 1);
          lambda.command->at(effect) = [](LambdaStatEffect::Args&) {
            //TODO: how does this work?
            //if(a.resolvedID != StableElementID::invalid()) {
            //  assert(a.resolvedID.toPacked<GameDatabase>().getTableIndex() == GameDatabase::getTableIndex<TargetPosTable>().getTableIndex());
            //  TableOperations::stableSwapRemove(std::get<TargetPosTable>(a.db->db.mTables), a.resolvedID.toPacked<GameDatabase>(), TableAdapters::getStableMappings(*a.db));
            //}
          };
        }
      });
    }
  };

  template<class... States>
  void createBuckets(IAppBuilder& builder, StateMachine<States...> sm) {
    using SM = decltype(sm);
    using StatesRow = typename SM::StateRow;
    using GlobalState = typename SM::GlobalStateRow;
    for(const UnpackedDatabaseElementID& table : builder.queryTables<StatesRow, GlobalState>().matchingTableIDs) {
      auto task = builder.createTask();
      task.setName("create state buckets");
      auto query = task.query<StatesRow, GlobalState>(table);

      task.setCallback([query](AppTaskArgs&) mutable {
        auto&& [states, global] = query.get(0);
        auto& buckets = global->at().buckets;

        for(size_t i = 0; i < buckets.size(); ++i) {
          buckets[i].clear();
        }

        for(size_t i = 0; i < states->size(); ++i) {
          auto& state = states->at(i);
          //See if there is a desired state transition
          if(auto desired = tryTakeDesiredState(state)) {
            //Exit the old state
            buckets[state.currentState.index()].exiting.push_back(i);
            buckets[desired->index()].entering.push_back(i);
            //Swap to new state and enter it
            state.currentState = std::move(*desired);
          }
          else {
            buckets[state.currentState.index()].updating.push_back(i);
          }
        }
      });

      builder.submitTask(std::move(task));
    }
  }

  template<class... States, size_t... I>
  void updateStates(IAppBuilder& builder, StateMachine<States...> sm, std::index_sequence<I...>) {
    using SM = decltype(sm);
    for(const UnpackedDatabaseElementID& table : builder.queryTables<typename SM::StateRow, typename SM::GlobalStateRow>().matchingTableIDs) {
      (StateTraits<States>::onExit(builder, table, I), ...);
      (StateTraits<States>::onEnter(builder, table, I), ...);
      (StateTraits<States>::onUpdate(builder, table, I), ...);
    }
  }

  template<class... States, size_t... I>
  void exitStates(IAppBuilder& builder, StateMachine<States...> sm, std::index_sequence<I...>) {
    using SM = decltype(sm);
    for(const UnpackedDatabaseElementID& table : builder.queryTables<typename SM::StateRow, typename SM::GlobalStateRow>().matchingTableIDs) {
      (StateTraits<States>::onExit(builder, table, I), ...);
    }
  }

  void update(IAppBuilder& builder) {
    FragmentStateMachineT sm;
    createBuckets(builder, sm);
    updateStates(builder, sm, sm.INDICES);
  }

  void processMigrations(IAppBuilder& builder) {
    //First clear all buckets
    {
      auto task = builder.createTask();
      task.setName("clear buckets");
      auto query = task.query<GlobalsRow>();
      task.setCallback([query](AppTaskArgs&) mutable {
        query.forEachElement([](auto& element) {
          for(size_t i = 0; i < element.buckets.size(); ++i) {
            element.buckets[i].clear();
          }
        });
      });
      builder.submitTask(std::move(task));
    }

    auto task = builder.createTask();
    task.setName("process state migrations");
    auto resolver = task.getResolver<StateRow, GlobalsRow>();
    auto ids = task.getIDResolver();
    const DBEvents& events = Events::getPublishedEvents(task);

    //If a goal would be lost, trigger the exit callback by transitioning to the empty state
    //and putting it in the exiting bucket of the state it came from
    task.setCallback([resolver, ids, &events](AppTaskArgs&) mutable {
      CachedRow<StateRow> fromState, toState;
      CachedRow<GlobalsRow> globals;
      for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
        auto fromTable = ids->uncheckedUnpack(cmd.source);
        auto toTable = ids->uncheckedUnpack(cmd.destination);
        resolver->tryGetOrSwapRow(fromState, fromTable);
        resolver->tryGetOrSwapRow(toState, toTable);
        if(fromState && !toState) {
          resolver->tryGetOrSwapRow(globals, fromTable);
          assert(globals);

          auto& currentState = fromState->at(fromTable.getElementIndex()).currentState;
          const size_t stateIndex = currentState.index();
          globals->at().buckets[stateIndex].exiting.push_back(stateIndex);
          currentState = Empty{};
        }
      }
    });

    builder.submitTask(std::move(task));

    //Process the exits potentially enqueued above
    FragmentStateMachineT sm;
    exitStates(builder, sm, sm.INDICES);
  }

  void preProcessEvents(IAppBuilder& builder) {
    processMigrations(builder);
  }
}