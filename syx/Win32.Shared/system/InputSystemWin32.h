#pragma once

#include "system/System.h"

enum class KeyState : uint8_t;
enum class Key : uint8_t;

class InputSystemWin32 : public System {
public:
  InputSystemWin32(const SystemArgs& args);

  virtual void queueTasks(float, IWorkerPool&, std::shared_ptr<Task>) override;

  std::optional<LRESULT> _mainProc(HWND wnd, UINT msg, WPARAM w, LPARAM l);

private:
  void _enqueueKeyEvent(Key key, KeyState state);
  void _enqueueMouseKeyEvent(Key key, KeyState state, const Syx::Vec2& position);
  void _enqueueMouseMove(const Syx::Vec2& pos, const Syx::Vec2& delta);
  void _enqueueMouseWheel(float amount);

  //Messages pending from the win32 update until the next engine update
  std::unique_ptr<EventBuffer> mPendingMessages;
  std::mutex mPendingMessagesMutex;
};