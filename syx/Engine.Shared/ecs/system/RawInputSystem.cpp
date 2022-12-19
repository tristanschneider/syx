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

  using RawInputView = View<Read<RawInputBufferComponent>, Read<RawInputComponent>>;
  using MappingsView = View<Write<InputMappingComponent>>;
  void tickUpdateInputMappings(SystemContext<RawInputView, MappingsView>& context) {
    struct MappingsVisitor {
      static void visit(const RawInputEvent::KeyEvent& e, InputMappingComponent& mappings) {
        for(auto&& m : mappings.mKeyMappings) {
          if(e.mKey == m.mFrom && e.mState == m.mState) {
            mappings.mKeyEvents.push_back({ m.mActionIndex });
          }
        }
      }

      static void visit(const RawInputEvent::MouseKeyEvent& e, InputMappingComponent& mappings) {
        for(auto&& m : mappings.mKeyMappings) {
          if(e.mKey == m.mFrom && e.mState == m.mState) {
            mappings.mKeyEvents.push_back({ m.mActionIndex });
          }
        }
      }

      static void visit(const RawInputEvent::MouseMoveEvent& e, InputMappingComponent& mappings) {
        for(auto&& m : mappings.mMappings2D) {
          //TODO: this doesn't really make any sense
          if(m.mSource == &RawInputComponent::mMouseDelta) {
            mappings.mEvents2D.push_back({ e.mDelta, m.mActionIndex });
          }
          else if(m.mSource == &RawInputComponent::mMousePos) {
            mappings.mEvents2D.push_back({ e.mPos, m.mActionIndex });
          }
        }
      }

      static void visit(const RawInputEvent::MouseWheelEvent& e, InputMappingComponent& mappings) {
        for(auto&& m : mappings.mMappings1D) {
          if(m.mSource == &RawInputComponent::mWheelDelta) {
            mappings.mEvents1D.push_back({ e.mAmount, m.mActionIndex });
          }
        }
      }

      static void visit(const RawInputEvent::TextEvent&, InputMappingComponent&) {}
    };

    //For each input source, process all mappings
    //If there's enough input and mappings there's likely a more clever way to do this but at current volume it doesn't matter
    for(auto inputEntity : context.get<RawInputView>()) {
      const RawInputBufferComponent& inputBuffer = inputEntity.get<const RawInputBufferComponent>();
      const RawInputComponent& inputState = inputEntity.get<const RawInputComponent>();

      for(auto entity : context.get<MappingsView>()) {
        //Down events are unique because they are the lack of a change to input state.
        //For that, look at the stored raw input to see if mapped keys are down
        //Since this is before the RawInputSystem it's last frame's input which allows
        //The first "Down" event to come the frame before the triggered event
        //TODO: this also means Down and Released mappings would trigger on the same frame
        InputMappingComponent& mappings = entity.get<InputMappingComponent>();
        for(const KeyMapping& mapping : mappings.mKeyMappings) {
          switch(mapping.mState) {
          case KeyState::Down:
          case KeyState::Up:
            if(inputState.getKeyState(mapping.mFrom) == mapping.mState) {
              mappings.mKeyEvents.push_back({ mapping.mActionIndex });
            }
            break;
          case KeyState::Invalid:
          case KeyState::Triggered:
          case KeyState::Released:
            break;
          }
        }
      }

      if(inputBuffer.mEvents.empty()) {
        continue;
      }

      for(auto entity : context.get<MappingsView>()) {
        InputMappingComponent& mappings = entity.get<InputMappingComponent>();
        for(const auto& input : inputBuffer.mEvents) {
          std::visit([&](const auto& e) {
            MappingsVisitor::visit(e, mappings);
          }, input.mData);
        }
      }
    }
  }
}

std::shared_ptr<Engine::System> RawInputSystem::init() {
  return ecx::makeSystem("InitInput", &tickInit);
}

std::shared_ptr<Engine::System> RawInputSystem::updateInputMappings() {
  return ecx::makeSystem("UpdateInputMappings", &tickUpdateInputMappings);
}

std::shared_ptr<Engine::System> RawInputSystem::update() {
  return ecx::makeSystem("UpdateInput", &tickUpdate);
}
