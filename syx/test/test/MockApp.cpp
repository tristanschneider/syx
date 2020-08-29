#include "Precompile.h"
#include "test/MockApp.h"

#include "App.h"
#include "test/TestAppPlatform.h"
#include "test/TestAppRegistration.h"

MockApp::MockApp()
  : mApp(std::make_unique<App>(std::make_unique<TestAppPlatform>(), std::make_unique<LuaRegistration>())) {
  mApp->init();
}

MockApp::~MockApp() {
  mApp->uninit();
}

App& MockApp::get() {
  return *mApp;
}
