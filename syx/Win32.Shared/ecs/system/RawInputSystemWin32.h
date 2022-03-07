#pragma once

#include "ecs/ECS.h"

struct RawInputSystemWin32 {
  static std::shared_ptr<Engine::System> init();
  //Update the InputEventBufferComponent with the Win32 input events enqueued in mainProc
  static std::shared_ptr<Engine::System> update();

  static std::optional<LRESULT> mainProc(HWND wnd, UINT msg, WPARAM w, LPARAM l);
};