#include "Precompile.h"
#include "event/EventHandler.h"

#include "event/Event.h"
#include "event/EventBuffer.h"

EventHandler::CallbackHandler EventHandler::_wrapListener(std::weak_ptr<EventListener> listener) {
  return {
    [listener](const Event& e) {
      if(auto self = listener.lock()) {
        self->onEvent(e);
      }
    },
    listener
  };
}

void EventHandler::registerEventListener(size_t type, std::weak_ptr<EventListener> listener) {
  _registerEventListener(type, _wrapListener(std::move(listener)));
}

void EventHandler::registerGlobalListener(std::weak_ptr<EventListener> listener) {
  mGlobalHandler.mHandlers.push_back(_wrapListener(listener));
}

void EventHandler::_registerEventListener(size_t type, CallbackHandler handler) {
  mEventHandlers[type].mHandlers.push_back(std::move(handler));
}

void EventHandler::HandlerSlot::invoke(const Event& e) {
  //Invoke handlers and remove expired elements. Use standard removal to preserve registration order
  for(size_t i = 0; i < mHandlers.size();) {
    CallbackHandler& handler = mHandlers[i];
    if(auto self = handler.mListener.lock()) {
      handler.mInvoker(e);
      ++i;
    }
    else {
      //Erase this and don't increment index since elements were shifted down
      mHandlers.erase(mHandlers.begin() + i);
    }
  }
}

void EventHandler::handleEvents(const EventBuffer& buffer) {
  for(const Event& e : buffer) {
    if(HandlerSlot* handler = mEventHandlers.get(e.getType())) {
      handler->invoke(e);
    }
    else {
      mGlobalHandler.invoke(e);
    }
  }
}