#include "Precompile.h"
#include "editor/event/EditorEvents.h"

DEFINE_EVENT(PickObjectEvent, Handle obj)
  , mObj(obj) {
}