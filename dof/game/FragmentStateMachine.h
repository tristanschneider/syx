#pragma once

#include "Table.h"

struct TaskRange;
struct GameDB;

namespace FragmentStateMachine {
  //Do nothing
  struct Idle {};
  //Navigate around in an arbitrary and varying direction
  struct Wander {};
  //Happens when hit by the player, fly in a given direction with collision disabled,
  //repelling all nearby fragments, then go to SeekHome
  struct Stunned {};
  //Navigate towards the destination location, then go back to wander or snap to destination if found
  struct SeekHome {};

  struct FragmentState {
    using Variant = std::variant<Idle, Wander, Stunned>;
    //Only used by the state machine itself
    Variant currentState;
    //Used by external logic to request changes for when the state machine updates
    std::mutex desiredStateMutex;
    Variant desiredState;
  };

  struct StateRow : Row<FragmentState> {};

  //Used by external code to request a state change during the next update
  //Can be called while the state machine is updating in which case it's up to timing if
  //it happens this tick or next
  void setState(FragmentState& current, FragmentState::Variant&& desired);

  //Process requested state transitions and update the logic of the current state for all
  //tables with `StateRow`s
  TaskRange update(GameDB db);
};