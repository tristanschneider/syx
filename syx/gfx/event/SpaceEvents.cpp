#include "Precompile.h"
#include "event/SpaceEvents.h"

DEFINE_EVENT(ClearSpaceEvent, Handle space)
  , mSpace(space) {
}

DEFINE_EVENT(LoadSpaceEvent, Handle space, const FilePath& file)
  , mSpace(space)
  , mFile(std::move(file)) {
}

DEFINE_EVENT(SaveSpaceEvent, Handle space, const FilePath& file)
  , mSpace(space)
  , mFile(std::move(file)) {
}
