#include "Precompile.h"
#include "system/InputSystemWin32.h"

#include "event/EventBuffer.h"
#include "event/InputEvents.h"
#include "provider/MessageQueueProvider.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"

InputSystemWin32::InputSystemWin32(const SystemArgs& args)
  : System(args, System::_typeId<InputSystemWin32>())
  , mPendingMessages(std::make_unique<EventBuffer>()) {

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

void InputSystemWin32::queueTasks(float, IWorkerPool& pool, std::shared_ptr<Task> updateTask) {
  //Create a task that copies the pending inputs to the main message queue, then clears the pending messages
  auto task = std::make_shared<FunctionTask>([this] {
    std::scoped_lock<std::mutex> lock(mPendingMessagesMutex);
    mPendingMessages->appendTo(*mArgs.mMessages->getMessageQueue());
    mPendingMessages->clear();
  });
  task->then(updateTask);
  pool.queueTask(task);
}

void InputSystemWin32::_enqueueKeyEvent(Key key, KeyState state) {
  KeyEvent e;
  e.mKey = key;
  e.mState = state;
  std::scoped_lock<std::mutex> lock(mPendingMessagesMutex);
  mPendingMessages->push(e);
}

void InputSystemWin32::_enqueueMouseKeyEvent(Key key, KeyState state, const Syx::Vec2& position) {
  MouseKeyEvent e;
  e.mKey = key;
  e.mState = state;
  e.mPos = position;
  std::scoped_lock<std::mutex> lock(mPendingMessagesMutex);
  mPendingMessages->push(e);
}

void InputSystemWin32::_enqueueMouseMove(const Syx::Vec2& pos, const Syx::Vec2& delta) {
  MouseMoveEvent e;
  e.mPos = pos;
  e.mDelta = delta;
  std::scoped_lock<std::mutex> lock(mPendingMessagesMutex);
  mPendingMessages->push(e);
}

void InputSystemWin32::_enqueueMouseWheel(float amount) {
  MouseWheelEvent e;
  e.mAmount = amount;
  std::scoped_lock<std::mutex> lock(mPendingMessagesMutex);
  mPendingMessages->push(e);
}

namespace {
  Key _mapWin32Key(WPARAM w) {
    //Key enum matches win32 virtual codes, so nothing special is needed
    return static_cast<Key>(w);
  }

  Syx::Vec2 _toPos(LPARAM l) {
    return Syx::Vec2(static_cast<float>(LOWORD(l)), static_cast<float>(HIWORD(l)));
  }
}

std::optional<LRESULT> InputSystemWin32::_mainProc(HWND, UINT msg, WPARAM w, LPARAM l) {
  switch(msg) {
  case WM_KEYDOWN:
    _enqueueKeyEvent(_mapWin32Key(w), KeyState::Triggered);
    // Circumvent normal key handling to prevent repeat events
    return std::make_optional(LRESULT(0));

  case WM_KEYUP:
    _enqueueKeyEvent(_mapWin32Key(w), KeyState::Released);
    return std::make_optional(LRESULT(0));

  case WM_LBUTTONDOWN:
    _enqueueMouseKeyEvent(Key::Left, KeyState::Triggered, _toPos(l));
    break;

  case WM_LBUTTONUP:
    _enqueueMouseKeyEvent(Key::Left, KeyState::Released, _toPos(l));
    break;

  case WM_RBUTTONDOWN:
    _enqueueMouseKeyEvent(Key::RightMouse, KeyState::Triggered, _toPos(l));
    break;

  case WM_RBUTTONUP:
    _enqueueMouseKeyEvent(Key::RightMouse, KeyState::Released, _toPos(l));
    break;

  case WM_MBUTTONDOWN:
    _enqueueMouseKeyEvent(Key::MiddleMouse, KeyState::Triggered, _toPos(l));
    break;

  case WM_MBUTTONUP:
    _enqueueMouseKeyEvent(Key::MiddleMouse, KeyState::Released, _toPos(l));
    break;

  case WM_XBUTTONDOWN:
    _enqueueMouseKeyEvent(Key::XMouse1, KeyState::Triggered, _toPos(l));
    break;

  case WM_XBUTTONUP:
    _enqueueMouseKeyEvent(Key::XMouse1, KeyState::Released, _toPos(l));
    break;

  case WM_MOUSEWHEEL:
    _enqueueMouseWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(w)));
    break;

  case WM_MOUSEMOVE:
    printf("mousemove\n");
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
          //This is a bitfield but RELATIVE is 0, so I'm assuming that relative movement is never combined with other values
          if(mouseInput.usFlags == MOUSE_MOVE_RELATIVE) {
            mouseDelta = Syx::Vec2(static_cast<float>(mouseInput.lLastX), static_cast<float>(mouseInput.lLastY));
            POINT result;
            ::GetCursorPos(&result);
            mousePos = Syx::Vec2(static_cast<float>(result.x), static_cast<float>(result.y));
          }
          else if(mouseInput.usFlags & MOUSE_MOVE_ABSOLUTE) {
            mousePos = Syx::Vec2(static_cast<float>(mouseInput.lLastX), static_cast<float>(mouseInput.lLastY));
            //TODO: compute delta by storing previous position? Could lead to edge cases when mouse snaps or leaves client area
          }
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