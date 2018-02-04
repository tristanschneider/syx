#include "Precompile.h"
#include "event/EventHandler.h"

#include "event/Event.h"
#include "event/EventBuffer.h"

void EventHandler::registerEventHandler(size_t type, Callback h) {
  assert((!mEventHandlers.get(type) || !*mEventHandlers.get(type)) && "Only one listener per type allowed");
  mEventHandlers[type] = h;
}

void EventHandler::registerGlobalHandler(Callback h) {
  assert((mGlobalHandler || !h) && "Only one global handler, so only valid to set it or clear it");
  mGlobalHandler = h;
}

void EventHandler::handleEvents(const EventBuffer& buffer) {
  for(const Event& e : buffer) {
    const Callback* handler = mEventHandlers.get(e.getType());

    if(handler && *handler)
      (*handler)(e);
    else if(mGlobalHandler)
      mGlobalHandler(e);
  }
}