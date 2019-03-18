#include "Precompile.h"
#include "event/ViewportEvents.h"

DEFINE_EVENT(SetViewportEvent, Viewport viewport)
  , mViewport(std::move(viewport)) {
}

DEFINE_EVENT(RemoveViewportEvent, std::string name)
  , mName(std::move(name)) {
}