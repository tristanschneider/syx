#include "Precompile.h"
#include "ecs/system/RawInputSystem.h"

#include "ecs/component/RawInputComponent.h"

namespace {
  using namespace Engine;
  void tickInit(SystemContext<EntityFactory>& context) {
    context.get<EntityFactory>().createEntityWithComponents<RawInputBufferComponent, RawInputComponent>();
  }

  void _tickKeyStates(RawInputComponent& input) {
    input.mText.clear();
    input.mWheelDelta = 0;
    input.mMouseDelta = Syx::Vec2(0);
    for(auto it = input.mKeyStates.begin(); it != input.mKeyStates.end();) {
      switch(it->second) {
        case KeyState::Up:
        case KeyState::Released:
          it = input.mKeyStates.erase(it);
          break;

        case KeyState::Triggered:
        default:
          it->second = KeyState::Down;
          ++it;
          break;
      }
    }
  }

  void onKeyStateChange(RawInputComponent& input, Key key, KeyState state) {
    //Update the state in the map, removing (or preventing creation) if the state is up, since absence from the map implies up
    auto it = input.mKeyStates.find(key);
    if(it != input.mKeyStates.end()) {
      if(state != KeyState::Up) {
        it->second = state;
      }
      else {
        input.mKeyStates.erase(it);
      }
    }
    else if(state != KeyState::Up) {
      input.mKeyStates[key] = state;
    }
  }

  void onRawInputEvent(const RawInputEvent::KeyEvent& e, RawInputComponent& input) {
    onKeyStateChange(input, e.mKey, e.mState);
  }

  void onRawInputEvent(const RawInputEvent::MouseKeyEvent& e, RawInputComponent& input) {
    onKeyStateChange(input, e.mKey, e.mState);
    input.mMousePos = e.mPos;
  }

  void onRawInputEvent(const RawInputEvent::MouseMoveEvent& e, RawInputComponent& input) {
    input.mMousePos = e.mPos;
    input.mMouseDelta = e.mDelta;
  }

  void onRawInputEvent(const RawInputEvent::MouseWheelEvent& e, RawInputComponent& input) {
    input.mWheelDelta = e.mAmount;
  }

  void onRawInputEvent(const RawInputEvent::TextEvent& e, RawInputComponent& input) {
    input.mText += e.mText;
  }

  using UpdateView = View<Write<RawInputBufferComponent>, Write<RawInputComponent>>;
  void tickUpdate(SystemContext<UpdateView>& context) {
    for(auto entity : context.get<UpdateView>()) {
      std::vector<RawInputEvent>& events = entity.get<RawInputBufferComponent>().mEvents;
      RawInputComponent& input = entity.get<RawInputComponent>();

      //Update deltas
      _tickKeyStates(input);

      //Process new events
      for(const RawInputEvent& event : events) {
        std::visit([&input](const auto& e) {
          onRawInputEvent(e, input);
        }, event.mData);
      }
      events.clear();
    }
  }
}

std::shared_ptr<Engine::System> RawInputSystem::init() {
  return ecx::makeSystem("InitInput", &tickInit);
}

std::shared_ptr<Engine::System> RawInputSystem::update() {
  return ecx::makeSystem("UpdateInput", &tickUpdate);
}
