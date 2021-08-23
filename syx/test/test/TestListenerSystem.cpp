#include "Precompile.h"
#include "test/TestListenerSystem.h"

#include "event/Event.h"
#include "event/EventHandler.h"
#include "test/TestAppRegistration.h"

TestListenerSystem::TestListenerSystem(const SystemArgs& args)
  : System(args, _typeId<TestListenerSystem>())
  , mEventBufferCopy(std::make_unique<EventBuffer>()) {
}

TestListenerSystem::~TestListenerSystem() = default;

void TestListenerSystem::init() {
  mEventHandler = std::make_unique<EventHandler>();
  mListeners.push_back(mEventHandler->registerEventListener(CallbackEvent::getHandler(LuaRegistration::TEST_CALLBACK_ID)));
  mEventHandler->registerGlobalListener(shared_from_this());
}

void TestListenerSystem::onEvent(const Event& e) {
    //Go through all handlers that match the type, removing if single-use, and preventing further handlers if they have a "Stop" response.
    for(auto it = mHandlers.begin(); it != mHandlers.end();) {
      if(it->mEventType == e.getType()) {
        if(it->mCallback) {
          it->mCallback(e, *mArgs.mMessages);
        }

        const HandlerResponse response = it->mResponse;
        if(it->mLifetime == HandlerLifetime::SingleUse) {
          it = mHandlers.erase(it);
        }
        else {
          ++it;
        }

        if(response == HandlerResponse::Stop) {
          break;
        }
      }
      else {
        ++it;
      }
    }
}

void TestListenerSystem::update(float, IWorkerPool&, std::shared_ptr<Task>) {
  mEventHandler->handleEvents(*mEventBuffer);

  mEventBufferCopy->clear();
  mEventBuffer->appendTo(*mEventBufferCopy);
}

const EventBuffer& TestListenerSystem::getEventBuffer() const {
  return *mEventBufferCopy;
}

bool TestListenerSystem::hasEventOfType(size_t type) const {
  return tryGetEventOfType(type) != nullptr;
}

const Event* TestListenerSystem::tryGetEventOfType(size_t type) const {
  auto it = std::find_if(mEventBufferCopy->begin(), mEventBufferCopy->end(), [type](const Event& e) { return e.getType() == type; });
  return it != mEventBufferCopy->end() ? &*it : nullptr;
}

TestListenerSystem& TestListenerSystem::registerEventHandler(size_t eventType, HandlerLifetime lifetime, HandlerResponse response, HandlerCallback callback) {
  mHandlers.push_back({ lifetime, response, std::move(callback), eventType });
  return *this;
}

TestListenerSystem& TestListenerSystem::clearEventHandlers() {
  mHandlers.clear();
  return *this;
}
