#include "Precompile.h"
#include "test/MockApp.h"

#include "App.h"
#include "CppUnitTest.h"
#include "test/TestAppPlatform.h"
#include "test/TestAppRegistration.h"

MockApp::MockApp()
  : MockApp(std::make_unique<TestAppPlatform>(), std::make_unique<LuaRegistration>()) {
}

MockApp::MockApp(std::unique_ptr<AppPlatform> appPlatform, std::unique_ptr<AppRegistration> registration)
  : mApp(std::make_unique<App>(std::move(appPlatform), std::move(registration))) {
  mApp->init();
}

MockApp::~MockApp() {
  mApp->uninit();
}

App& MockApp::get() {
  return *mApp;
}

void MockApp::waitUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout) {
  auto startTime = std::chrono::steady_clock::now();
  //To make local debugging with breakpoints less annoying, only start obeying the timeout after 100 loop iterations
  for (size_t attempt = 0; attempt < 100 || std::chrono::steady_clock::now() - startTime < timeout; ++attempt) {
    if(condition()) {
      return;
    }
    //Unrealistically fast dt, doesn't matter at the moment
    mApp->update(1.f);
  }
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail(L"Timed out waiting for condition");
}
