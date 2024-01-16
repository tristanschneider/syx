#pragma once

#include "StableElementID.h"
#include "StateMachine.h"
#include "glm/vec2.hpp"
#include "generics/Timer.h"

class IAppBuilder;
struct TaskRange;

namespace FragmentStateMachine {
  //Do nothing
  struct Idle {};
  //Navigate around in an arbitrary and varying direction
  struct Wander {
    StableElementID spatialQuery;
    glm::vec2 desiredDirection{};
  };
  //Happens when hit by the player, fly in a given direction with collision disabled,
  //repelling all nearby fragments, then go to SeekHome
  struct Stunned {};
  //Navigate towards the destination location, then go back to wander or snap to destination if found
  struct SeekHome {
    gnx::time::Timer timer;
  };
  //Continuation of seekHome, stop applying the impulse and do spatial queries until there are
  //no collisions so the mask can be set to collide again
  struct ExitSeekHome {
    StableElementID spatialQuery;
    glm::vec2 direction{};
  };
  //State that does nothing. Used by destruction to exit the final state without entering anything new
  struct Empty {};

  using FragmentStateMachineT = SM::StateMachine<Idle, Wander, Stunned, SeekHome, ExitSeekHome, Empty>;
  using FragmentState = typename FragmentStateMachineT::State;

  using StateRow = typename FragmentStateMachineT::StateRow;
  using GlobalsRow = typename FragmentStateMachineT::GlobalStateRow;

  //Used by external code to request a state change during the next update
  //Can be called while the state machine is updating in which case it's up to timing if
  //it happens this tick or next
  void setState(FragmentState& current, FragmentStateMachineT::Variant&& desired);

  //Process requested state transitions and update the logic of the current state for all
  //tables with `StateRow`s
  void update(IAppBuilder& builder);
  void preProcessEvents(IAppBuilder& builder);
};