#include "Precompile.h"
#include "test/TestListenerSystem.h"

#include "event/Event.h"
#include "event/EventHandler.h"
#include "test/TestAppRegistration.h"

TestListenerSystem::TestListenerSystem(const SystemArgs& args)
    : System(args, _typeId<TestListenerSystem>()) {
  }

void TestListenerSystem::init() {
  mEventHandler = std::make_unique<EventHandler>();
  mEventHandler->registerEventHandler(CallbackEvent::getHandler(LuaRegistration::TEST_CALLBACK_ID));
}

void TestListenerSystem::update(float, IWorkerPool&, std::shared_ptr<Task>) {
  mEventHandler->handleEvents(*mEventBuffer);
}

const EventBuffer& TestListenerSystem::getEventBuffer() const {
  return *mEventBuffer;
}

bool TestListenerSystem::hasEventOfType(size_t type) const {
  return std::any_of(mEventBuffer->begin(), mEventBuffer->end(), [type](const Event& e) { return e.getType() == type; });
}
