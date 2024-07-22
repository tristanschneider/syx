#pragma once

#include "StableElementID.h"
#include "AppBuilder.h"
#include "DBEvents.h"

namespace SM {
  struct StateBucket {
    void clear() {
      entering.clear();
      exiting.clear();
      updating.clear();
    }

    std::vector<size_t> entering, exiting, updating;
  };

  struct EmptyStateTraits {
    static void onEnter([[maybe_unused]] IAppBuilder& builder, [[maybe_unused]] const TableID& table, [[maybe_unused]] size_t bucket) {}
    static void onUpdate([[maybe_unused]] IAppBuilder& builder, [[maybe_unused]] const TableID& table, [[maybe_unused]] size_t bucket) {}
    static void onExit([[maybe_unused]] IAppBuilder& builder, [[maybe_unused]] const TableID& table, [[maybe_unused]] size_t bucket) {}
  };

  template<class T>
  struct StateTraits : EmptyStateTraits {};

  template<class... States>
  struct StateMachine {
    using Variant = std::variant<States...>;
    constexpr static size_t STATE_COUNT = sizeof...(States);
    constexpr static auto INDICES = std::index_sequence_for<States...>{};

    struct State {
      using Variant = StateMachine::Variant;
      State() = default;
      State(const Variant& v)
        : currentState(v)
        , desiredState(v) {
      }
      State(State&& rhs)
        : currentState(std::move(rhs.currentState))
        , desiredState(std::move(rhs.desiredState)) {
      }

      void operator=(State&& rhs) {
        currentState = std::move(rhs.currentState);
        desiredState = std::move(rhs.desiredState);
      }

      //Only used by the state machine itself
      Variant currentState;
      //Used for onEnter and onExit transitions
      Variant previousState;
      //Used by external logic to request changes for when the state machine updates
      std::mutex desiredStateMutex;
      Variant desiredState;
    };

    struct GlobalState {
      std::array<StateBucket, STATE_COUNT> buckets;
    };

    struct StateRow : Row<State> {};
    struct GlobalStateRow : SharedRow<GlobalState> {};
  };

  template<class StateT>
  std::optional<typename StateT::Variant> tryTakeDesiredState(StateT& current) {
    std::lock_guard<std::mutex> lock{ current.desiredStateMutex };
    return current.currentState.index() != current.desiredState.index() ? std::make_optional(current.desiredState) : std::nullopt;
  }

  template<class... States>
  void createBuckets(IAppBuilder& builder, StateMachine<States...> sm) {
    using Machine = decltype(sm);
    using StatesRow = typename Machine::StateRow;
    using GlobalState = typename Machine::GlobalStateRow;
    for(const TableID& table : builder.queryTables<StatesRow, GlobalState>().matchingTableIDs) {
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
            state.previousState = state.currentState;
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
    for(const TableID& table : builder.queryTables<typename SM::StateRow, typename SM::GlobalStateRow>().matchingTableIDs) {
      (StateTraits<States>::onExit(builder, table, I), ...);
      (StateTraits<States>::onEnter(builder, table, I), ...);
      (StateTraits<States>::onUpdate(builder, table, I), ...);
    }
  }

  template<class... States, size_t... I>
  void exitStates(IAppBuilder& builder, StateMachine<States...> sm, std::index_sequence<I...>) {
    using SM = decltype(sm);
    for(const TableID& table : builder.queryTables<typename SM::StateRow, typename SM::GlobalStateRow>().matchingTableIDs) {
      (StateTraits<States>::onExit(builder, table, I), ...);
    }
  }

  template<class State>
  void setState(State& current, typename State::Variant&& desired) {
    std::lock_guard<std::mutex> lock{ current.desiredStateMutex };
    current.desiredState = std::move(desired);
  }

  template<class Machine>
  void update(IAppBuilder& builder) {
    Machine sm;
    createBuckets(builder, sm);
    updateStates(builder, sm, sm.INDICES);
  }

  template<class Machine>
  void preProcessEvents(IAppBuilder& builder) {
    using GlobalsRow = typename Machine::GlobalStateRow;
    using StateRow = typename Machine::StateRow;
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
      auto res = ids->getRefResolver();
      for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
        const ElementRef* source = std::get_if<ElementRef>(&cmd.source);
        const TableID* destination = std::get_if<TableID>(&cmd.destination);
        //Not a move we care about
        if(!source || !destination) {
          continue;
        }
        const UnpackedDatabaseElementID sourceID = res.tryUnpack(*source).value_or(UnpackedDatabaseElementID{});
        resolver->tryGetOrSwapRow(fromState, sourceID);
        resolver->tryGetOrSwapRow(toState, *destination);
        if(fromState && !toState) {
          resolver->tryGetOrSwapRow(globals, sourceID);
          assert(globals);

          auto& state = fromState->at(sourceID.getElementIndex());
          auto& currentState = state.currentState;
          const size_t stateIndex = currentState.index();
          globals->at().buckets[stateIndex].exiting.push_back(sourceID.getElementIndex());
          state.previousState = currentState;
          currentState = {};
        }
      }
    });

    builder.submitTask(std::move(task));

    //Process the exits potentially enqueued above
    Machine sm;
    exitStates(builder, sm, sm.INDICES);
  }
}