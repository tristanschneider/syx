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
  mEventHandler->registerEventHandler(CallbackEvent::getHandler(LuaRegistration::TEST_CALLBACK_ID));
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
