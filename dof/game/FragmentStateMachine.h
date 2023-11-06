#pragma once

#include "StableElementID.h"

class IAppBuilder;
struct TaskRange;

namespace FragmentStateMachine {
  //Do nothing
  struct Idle {};
  //Navigate around in an arbitrary and varying direction
  struct Wander {
    StableElementID spatialQuery;
  };
  //Happens when hit by the player, fly in a given direction with collision disabled,
  //repelling all nearby fragments, then go to SeekHome
  struct Stunned {};
  //Navigate towards the destination location, then go back to wander or snap to destination if found
  struct SeekHome {
    StableElementID target;
  };
  //State that does nothing. Used by destruction to exit the final state without entering anything new
  struct Empty {};

  struct StateBucket {
    void clear() {
      entering.clear();
      exiting.clear();
      updating.clear();
    }

    std::vector<size_t> entering, exiting, updating;
  };

  struct EmptyStateTraits {
    static void onEnter([[maybe_unused]] IAppBuilder& builder, [[maybe_unused]] const UnpackedDatabaseElementID& table, [[maybe_unused]] size_t bucket) {}
    static void onUpdate([[maybe_unused]] IAppBuilder& builder, [[maybe_unused]] const UnpackedDatabaseElementID& table, [[maybe_unused]] size_t bucket) {}
    static void onExit([[maybe_unused]] IAppBuilder& builder, [[maybe_unused]] const UnpackedDatabaseElementID& table, [[maybe_unused]] size_t bucket) {}
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

  using FragmentStateMachineT = StateMachine<Idle, Wander, Stunned, SeekHome, Empty>;
  using FragmentState = typename FragmentStateMachineT::State;

  using StateRow = FragmentStateMachineT::StateRow;
  using GlobalsRow = FragmentStateMachineT::GlobalStateRow;

  //Used by external code to request a state change during the next update
  //Can be called while the state machine is updating in which case it's up to timing if
  //it happens this tick or next
  void setState(FragmentState& current, FragmentStateMachineT::Variant&& desired);

  //Process requested state transitions and update the logic of the current state for all
  //tables with `StateRow`s
  void update(IAppBuilder& builder);
  void preProcessEvents(IAppBuilder& builder);
};