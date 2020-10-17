#pragma once

class App;
class EventHandler;

struct MockApp {
  MockApp();
  virtual ~MockApp();

  App& get();

  void waitUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

  App* operator->() {
    return mApp.get();
  }

  std::unique_ptr<App> mApp;
};
