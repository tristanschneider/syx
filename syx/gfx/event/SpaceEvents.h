#pragma once
#include "event/Event.h"

class ClearSpaceEvent : public Event {
public:
  ClearSpaceEvent(Handle sceneId);
  Handle mSpace;
};
