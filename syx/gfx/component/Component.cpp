#include "Precompile.h"
#include "component/Component.h"

#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "provider/MessageQueueProvider.h"

Component::Registry::Registrar::Registrar(size_t type, Constructor ctor) {
  Component::Registry::registerComponent(type, ctor);
}

void Component::Registry::registerComponent(size_t type, Constructor ctor) {
  _get().mCtors[type] = ctor;
}

std::unique_ptr<Component> Component::Registry::construct(size_t type, Handle owner) {
  return _get().mCtors[type](owner);
}

Component::Registry::Registry() {
}

Component::Registry& Component::Registry::_get() {
  static Registry singleton;
  return singleton;
}

Component::Component(Handle type, Handle owner)
  : mOwner(owner)
  , mType(type) {
}

Component::~Component() {
}