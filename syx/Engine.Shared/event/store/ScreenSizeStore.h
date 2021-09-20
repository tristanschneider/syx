#pragma once
#include "event/EventHandler.h"
#include "SyxVec2.h"

struct WindowResize;

struct ScreenSize {
  Syx::Vec2 mSize;
};

// A wrapper around screen size events to cache the current screen size
class ScreenSizeStore : public EventStoreImpl<ScreenSizeStore, ScreenSize, std::mutex> {
public:
  void init(EventHandler& handler) override;
private:
  void _onWindowResize(const WindowResize& e);
};