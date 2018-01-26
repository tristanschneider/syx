#include "Precompile.h"
#include "event/BaseComponentEvents.h"

DEFINE_EVENT(AddComponentEvent, Handle obj, Handle compType)
  , mObj(obj)
  , mCompType(compType) {
}

DEFINE_EVENT(RemoveComponentEvent, Handle obj, Handle compType)
  , mObj(obj)
  , mCompType(compType) {
}