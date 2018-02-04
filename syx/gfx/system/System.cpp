#include "Precompile.h"
#include "system/System.h"

#include "event/Event.h"
#include "event/EventHandler.h"

System::Registry::Registry() {
}

System::Registry::~Registry() {
}

size_t System::Registry::registerSystem(SystemConstructor systemConstructor) {
  Registry& r = _get();
  size_t id = r.mSystems.size();
  r.mSystems.push_back(systemConstructor);
  return id;
}

void System::Registry::getSystems(const SystemArgs& args, std::vector<std::unique_ptr<System>>& result) {
  result.clear();
  for(auto& constructor : _get().mSystems)
    result.emplace_back(std::move(constructor(args)));
}

System::Registry& System::Registry::_get() {
  static Registry singleton;
  return singleton;
}

System::System(const SystemArgs& args)
  : mArgs(args) {
}

System::~System() {
}

void System::setEventBuffer(const EventBuffer* buffer) {
  mEventBuffer = buffer;
}