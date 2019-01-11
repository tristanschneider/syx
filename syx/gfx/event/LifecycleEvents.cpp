#include "Precompile.h"
#include "event/LifecycleEvents.h"

DEFINE_EVENT(AllSystemsInitialized) {
}

DEFINE_EVENT(UriActivated, std::string uri)
  , mUri(std::move(uri)) {
}