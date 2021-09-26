#include "Precompile.h"
#include "system/System.h"

#include "event/Event.h"
#include "event/EventHandler.h"


class SystemRegistry : public ISystemRegistry {
public:
  void registerSystem(std::shared_ptr<System> system) override {
    mSystems.emplace_back(std::move(system));
  }

  std::vector<std::shared_ptr<System>> takeSystems() override {
    return std::move(mSystems);
  }

private:
  std::vector<std::shared_ptr<System>> mSystems;
};

System::System(const SystemArgs& args, typeId_t<System> type)
  : mArgs(args)
  , mType(type) {
}

System::~System() {
}

void System::setEventBuffer(const EventBuffer* buffer) {
  mEventBuffer = buffer;
}

MessageQueueProvider& System::getMessageQueueProvider() const {
  return *mArgs.mMessages;
}

namespace Registry {
  std::unique_ptr<ISystemRegistry> createSystemRegistry() {
    return std::make_unique<SystemRegistry>();
  }
}