#pragma once

#include "Table.h"
#include "glm/vec2.hpp"
#include <bitset>
#include "InputStateMachine.h"

namespace Ability {
  struct AbilityInput;
}
namespace Input {
  class InputMapper;
  class StateMachine;
  using EventID = uint32_t;
  using KeyMapID = uint32_t;
  using NodeIndex = uint32_t;
};
class IAppBuilder;

namespace GameInput {
  enum class KeyState : uint8_t {
    Up,
    Triggered,
    Released,
    Down,
  };

  namespace Keys {
    //GAME_X means a Virtual key computed by game logic not expected to come from a physical key
    constexpr Input::KeyMapID INIT_ONCE{ 0 };
    constexpr Input::KeyMapID MOVE_2D{ 1 };
    constexpr Input::KeyMapID ACTION_1{ 2 };
    constexpr Input::KeyMapID ACTION_2{ 3 };
    constexpr Input::KeyMapID DEBUG_ZOOM_1D{ 4 };
    constexpr Input::KeyMapID CHANGE_DENSITY_1D{ 5 };
    constexpr Input::KeyMapID GAME_ON_GROUND{ 6 };
    constexpr Input::KeyMapID JUMP{ 7 };
  };

  namespace Events {
    constexpr Input::EventID FULL_TRIGGER_ACTION_1{ 1 };
    constexpr Input::EventID PARTIAL_TRIGGER_ACTION_1{ 2 };
    constexpr Input::EventID CHANGE_DENSITY{ 3 };
    constexpr Input::EventID DEBUG_ZOOM{ 4 };
    constexpr Input::EventID JUMP{ 5 };
  };

  struct PlayerNodes {
    Input::InputSourceRange move2D{};
  };

  //Final desired move input state
  struct PlayerInput {
    PlayerInput();
    PlayerInput(PlayerInput&&);
    ~PlayerInput();

    PlayerInput& operator=(PlayerInput&&);

    bool wantsRebuild{};
    PlayerNodes nodes;
    //Goes from 0 to 1 when starting input in a direction, then back down to zero when stopping
    float moveT{};
    float angularMoveT{};

    std::unique_ptr<Ability::AbilityInput> ability1;
  };

  //Intermediate keyboard state used to compute final state
  struct PlayerKeyboardInput {
    enum class Key : uint8_t {
      Up,
      Down,
      Left,
      Right,
      Count,
    };
    std::bitset<(size_t)Key::Count> mKeys;
    glm::vec2 mRawMousePixels{};
    glm::vec2 mRawMouseDeltaPixels{};
    float mRawWheelDelta{};
    std::vector<std::pair<KeyState, int>> mRawKeys;
    std::string mRawText;
  };

  struct CameraDebugInput {
    bool needsInit{ true };
  };

  struct PlayerInputRow : Row<PlayerInput> {};
  struct PlayerKeyboardInputRow : SharedRow<PlayerKeyboardInput> {};
  //If any table has this the platform will feed inputs into it
  struct StateMachineRow : Row<Input::StateMachine> {};
  //This is created by the platform for the game to use to create state machines
  struct GlobalMappingsRow : SharedRow<Input::InputMapper> {};
  struct CameraDebugInputRow : Row<CameraDebugInput> {};

  //Create machines that never change
  void init(IAppBuilder& builder);
  //Clears the published events of all state machines and processes rebuild requests for the player state machine
  void update(IAppBuilder& builder);

  Input::Timespan secondsToTimespan(float seconds);
  float timespanToSeconds(Input::Timespan time);
}