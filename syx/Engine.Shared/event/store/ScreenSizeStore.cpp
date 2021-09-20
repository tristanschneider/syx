#include "Precompile.h"
#include "event/store/ScreenSizeStore.h"

#include "event/LifecycleEvents.h"

void ScreenSizeStore::init(EventHandler& handler) {
  handler.registerEventListener(_sharedFromThis(), &ScreenSizeStore::_onWindowResize);
}

void ScreenSizeStore::_onWindowResize(const WindowResize& e) {
  _set({ Syx::Vec2(static_cast<float>(e.mWidth), static_cast<float>(e.mHeight)) });
}
