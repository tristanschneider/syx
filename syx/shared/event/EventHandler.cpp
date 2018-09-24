#include "Precompile.h"
#include "event/EventHandler.h"

#include "event/Event.h"
#include "event/EventBuffer.h"

void EventHandler::registerEventHandler(size_t type, Callback h) {
  Callback* existingCallback = mEventHandlers.get(type);
  //If a callback already exists, chain the calls together
  if(existingCallback && *existingCallback) {
    Callback existingCopy = std::move(*existingCallback);
    mEventHandlers[type] = [cur = std::move(h), prev = std::move(existingCopy)](const Event& e) {
      prev(e);
      cur(e);
    };
  }
  else {
    mEventHandlers[type] = std::move(h);
  }
}

void EventHandler::registerGlobalHandler(Callback h) {
  //If a callback already exists and h isn't clearing the callbacks, chain the calls together
  if(mGlobalHandler && h) {
    Callback existingCopy = std::move(mGlobalHandler);
    mGlobalHandler = [cur = std::move(h), prev = std::move(existingCopy)](const Event& e) {
      prev(e);
      cur(e);
    };
  }
  else {
    mGlobalHandler = std::move(h);
  }
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