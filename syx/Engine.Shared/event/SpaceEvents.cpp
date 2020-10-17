#include "Precompile.h"
#include "event/SpaceEvents.h"

ClearSpaceEvent::ClearSpaceEvent(Handle space)
  : mSpace(space) {
}

LoadSpaceEvent::LoadSpaceEvent(Handle space, const FilePath& file)
  : mSpace(space)
  , mFile(std::move(file)) {
}

SaveSpaceEvent::SaveSpaceEvent(Handle space, const FilePath& file)
  : mSpace(space)
  , mFile(std::move(file)) {
}

SetTimescaleEvent::SetTimescaleEvent(Handle space, float timescale)
  : mSpace(space)
  , mTimescale(timescale) {
}