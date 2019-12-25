#pragma once
#include "event/Event.h"
#include "graphics/Viewport.h"

class SetViewportEvent : public Event {
public:
  SetViewportEvent(Viewport viewport);

  Viewport mViewport;
};

class RemoveViewportEvent : public Event {
public:
  RemoveViewportEvent(std::string name);

  std::string mName;
};