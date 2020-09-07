#pragma once

class App;

struct MockApp {
  MockApp();
  virtual ~MockApp();

  App& get();

  App* operator->() {
    return mApp.get();
  }

  std::unique_ptr<App> mApp;
};
