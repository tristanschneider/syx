#pragma once

class App;

struct MockApp {
  MockApp();
  virtual ~MockApp();

  App& get();

  std::unique_ptr<App> mApp;
};
