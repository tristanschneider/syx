#pragma once

class App;
class AppPlatform;
class AppRegistration;
class EventHandler;

struct MockApp {
  MockApp();
  MockApp(std::unique_ptr<AppPlatform> appPlatform, std::unique_ptr<AppRegistration> registration);
  MockApp(MockApp&&);
  virtual ~MockApp();

  App& get();

  void waitUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

  App* operator->() {
    return mApp.get();
  }

  App& operator*() {
    return *mApp;
  }

  std::unique_ptr<App> mApp;
};
