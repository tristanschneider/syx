#include "Precompile.h"
#include "ecs/system/RawInputSystemWin32.h"

#include "ecs/component/RawInputComponent.h"

//Can't use something exposed in ECS because this is used in the windows message loop
extern HWND gHwnd;

//TODO: integration tests for this using the Win32AppRegistration
namespace Input32Impl {
  using namespace Engine;

  //This state needs to be outside of ECS because the platform message thread is not part of the ECS update,
  //which is what updates the state. If more platform state was needed it would likely make sense to have
  //two registries and a copy step at a synchronization to transfer data between them.
  struct InputContext {
    std::mutex mMutex;
    std::vector<RawInputEvent> mEvents;
    bool mIsRelativeMouse = false;
    Syx::Vec2 mLastMousePos;
  };

  InputContext& getInputContext() {
    static InputContext input;
    return input;
  }

  void tickInit(SystemContext<>&) {
    std::array<RAWINPUTDEVICE, 1> devices;
    auto& mouse = devices[0];
    mouse.usUsagePage = 1;
    mouse.usUsage = 2;
    //The examples show using RIDEV_NOLEGACY but those events are needed for normal window interaction, so keep the flags empty
    mouse.dwFlags = 0;
    mouse.hwndTarget = 0;

    if(!RegisterRawInputDevices(devices.data(), static_cast<UINT>(devices.size()), sizeof(RAWINPUTDEVICE))) {
      const DWORD err = GetLastError();
      printf("Failed to register input with error %s\n", std::to_string(err).c_str());
    }
  }

  using UpdateView = View<Write<RawInputBufferComponent>>;
  void tickUpdate(SystemContext<UpdateView>& context) {
    for(auto entity : context.get<UpdateView>()) {
      InputContext& input = getInputContext();
      std::vector<RawInputEvent>& toUpdate = entity.get<RawInputBufferComponent>().mEvents;

      //Swap optimization is used under the assumption that this is the only thing pushing events and that they are all processed in a frame
      assert(toUpdate.empty() && "Input should be empty at this point or this optimization needs to be changed");

      //Publish the changes to the App's event buffer and take the empty buffer to fill for next frame
      std::scoped_lock<std::mutex> lock(input.mMutex);
      input.mEvents.swap(toUpdate);
    }
  }

  void _enqueueKeyEvent(InputContext& input, Key key, KeyState state) {
    std::scoped_lock<std::mutex> lock(input.mMutex);
    input.mEvents.push_back({ RawInputEvent::KeyEvent{ key, state } });
  }

  void _enqueueTextEvent(InputContext& input, std::string text) {
    std::scoped_lock<std::mutex> lock(input.mMutex);
    input.mEvents.push_back({ RawInputEvent::TextEvent{ std::move(text) } });
  }

  void _enqueueMouseKeyEvent(InputContext& input, Key key, KeyState state, const Syx::Vec2& position) {
    std::scoped_lock<std::mutex> lock(input.mMutex);
    input.mEvents.push_back({ RawInputEvent::MouseKeyEvent{ key, state, position } });
  }

  void _enqueueMouseMove(InputContext& input, const Syx::Vec2& pos, const Syx::Vec2& delta) {
    std::scoped_lock<std::mutex> lock(input.mMutex);
    input.mEvents.push_back({ RawInputEvent::MouseMoveEvent{ pos,delta } });
  }

  void _enqueueMouseWheel(InputContext& input, float amount) {
    std::scoped_lock<std::mutex> lock(input.mMutex);
    input.mEvents.push_back({ RawInputEvent::MouseWheelEvent{ amount } });
  }

  Key _mapWin32Key(WPARAM w) {
    //Key enum matches win32 virtual codes, so nothing special is needed
    return static_cast<Key>(w);
  }

  Syx::Vec2 _toPos(LPARAM l) {
    return Syx::Vec2(static_cast<float>(LOWORD(l)), static_cast<float>(HIWORD(l)));
  }
}

std::shared_ptr<Engine::System> RawInputSystemWin32::init() {
  return ecx::makeSystem("Input32Init", &Input32Impl::tickInit);
}

std::shared_ptr<Engine::System> RawInputSystemWin32::update() {
  return ecx::makeSystem("Input32Update", &Input32Impl::tickUpdate);
}

std::optional<LRESULT> RawInputSystemWin32::mainProc(HWND, UINT msg, WPARAM w, LPARAM l) {
  using namespace Input32Impl;
  InputContext& context = getInputContext();
  switch(msg) {
  case WM_KEYDOWN:
    _enqueueKeyEvent(context, _mapWin32Key(w), KeyState::Triggered);
    // Circumvent normal key handling to prevent repeat events
    return std::make_optional(LRESULT(0));

  case WM_KEYUP:
    _enqueueKeyEvent(context, _mapWin32Key(w), KeyState::Released);
    return std::make_optional(LRESULT(0));

  case WM_CHAR: {
    std::string str;
    str.push_back(static_cast<char>(w));
    _enqueueTextEvent(context, std::move(str));
    break;
  }

  case WM_LBUTTONDOWN:
    _enqueueMouseKeyEvent(context, Key::LeftMouse, KeyState::Triggered, _toPos(l));
    break;

  case WM_LBUTTONUP:
    _enqueueMouseKeyEvent(context, Key::LeftMouse, KeyState::Released, _toPos(l));
    break;

  case WM_RBUTTONDOWN:
    _enqueueMouseKeyEvent(context, Key::RightMouse, KeyState::Triggered, _toPos(l));
    break;

  case WM_RBUTTONUP:
    _enqueueMouseKeyEvent(context, Key::RightMouse, KeyState::Released, _toPos(l));
    break;

  case WM_MBUTTONDOWN:
    _enqueueMouseKeyEvent(context, Key::MiddleMouse, KeyState::Triggered, _toPos(l));
    break;

  case WM_MBUTTONUP:
    _enqueueMouseKeyEvent(context, Key::MiddleMouse, KeyState::Released, _toPos(l));
    break;

  case WM_XBUTTONDOWN:
    _enqueueMouseKeyEvent(context, Key::XMouse1, KeyState::Triggered, _toPos(l));
    break;

  case WM_XBUTTONUP:
    _enqueueMouseKeyEvent(context, Key::XMouse1, KeyState::Released, _toPos(l));
    break;

  case WM_MOUSEWHEEL:
    _enqueueMouseWheel(context, static_cast<float>(GET_WHEEL_DELTA_WPARAM(w)));
    break;

  case WM_MOUSEMOVE:
    break;

  case WM_INPUT: {
    //Input while in foreground
    if(w == RIM_INPUT) {
      std::array<uint8_t, 1024> buffer;
      UINT bufferSize = static_cast<UINT>(buffer.size());
      if(::GetRawInputData(reinterpret_cast<HRAWINPUT>(l), RID_INPUT, buffer.data(), &bufferSize, sizeof(RAWINPUTHEADER))) {
        RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.data());

        //Currently only registered for mouse events anyway with RegisterRawInputDevices
        if(input->header.dwType == RIM_TYPEMOUSE) {
          const RAWMOUSE& mouseInput = input->data.mouse;
          Syx::Vec2 mousePos, mouseDelta;
          std::optional<bool> newIsRelative;
          //This is a bitfield but RELATIVE is 0, so I'm assuming that relative movement is never combined with other values
          if(mouseInput.usFlags == MOUSE_MOVE_RELATIVE) {
            newIsRelative = true;
            mouseDelta = Syx::Vec2(static_cast<float>(mouseInput.lLastX), static_cast<float>(mouseInput.lLastY));
            POINT result;
            ::GetCursorPos(&result);
            mousePos = Syx::Vec2(static_cast<float>(result.x), static_cast<float>(result.y));
          }
          else if(mouseInput.usFlags & MOUSE_MOVE_ABSOLUTE) {
            newIsRelative = false;
            mousePos = Syx::Vec2(static_cast<float>(mouseInput.lLastX), static_cast<float>(mouseInput.lLastY));
          }

          if(newIsRelative) {
            POINT p{ (LONG)mousePos.x, (LONG)mousePos.y };
            //TODO: could this use the provided hwnd instead of global?
            if(::ScreenToClient(gHwnd, &p)) {
              mousePos.x = (float)p.x;
              mousePos.y = (float)p.y;
            }

            //If relative mode was the same as last time, compute the delta normally
            //Read outside of mutex is fine here since this update is the only thing that uses these values
            if(*newIsRelative == context.mIsRelativeMouse) {
              if(!context.mIsRelativeMouse) {
                mouseDelta = mousePos - context.mLastMousePos;
                context.mLastMousePos = mousePos;
              }
            }
            //Relative mouse mode changed, don't compute a delta as it may be dramatic, set last pos for potential use next input
            else {
              context.mLastMousePos = mousePos;
              context.mIsRelativeMouse = *newIsRelative;
            }
          }
          _enqueueMouseMove(context, mousePos, mouseDelta);
        }

        //Documentation says this does nothing. Call it because it seems reasonable
        ::DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
      }
    }
    //Mark this event as handled
    return 0;
  }
  }

  return {};
}
