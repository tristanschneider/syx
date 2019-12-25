#include "Precompile.h"
#include "event/TransformEvent.h"

DEFINE_EVENT(TransformEvent, Handle handle, Syx::Mat4 transform, size_t fromSystem)
  , mHandle(handle)
  , mTransform(transform)
  , mFromSystem(fromSystem) {
}