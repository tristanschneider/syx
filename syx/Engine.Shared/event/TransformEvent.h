#pragma once
#include "event/Event.h"

class TransformEvent : public Event {
public:
  TransformEvent(Handle handle, Syx::Mat4 transform, size_t fromSystem = static_cast<size_t>(-1));

  Handle mHandle;
  Syx::Mat4 mTransform;
  size_t mFromSystem;
};
