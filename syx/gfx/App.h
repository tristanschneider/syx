#pragma once
#include "system/System.h"

class GraphicsSystem;
class KeyboardInput;
class System;
class Space;
enum class SystemId : uint8_t;
class IWorkerPool;
class AppPlatform;

class App {
public:
  App(std::unique_ptr<AppPlatform> appPlatform);
  ~App();

  void init();
  void update(float dt);
  void uninit();
  Space& getDefaultSpace();
  IWorkerPool& getWorkerPool();
  AppPlatform& getAppPlatform();

  template<typename T>
  T* getSystem() {
    size_t id = GetSystemID(T);
    return id < mSystems.size() ? static_cast<T*>(mSystems[id].get()) : nullptr;
  }

  //Temporary until asset manager that wraps asset loading and such
  std::unordered_map<std::string, Handle> mAssets;

private:
  std::vector<std::unique_ptr<System>> mSystems;
  std::unique_ptr<Space> mDefaultSpace;
  std::unique_ptr<IWorkerPool> mWorkerPool;
  std::unique_ptr<AppPlatform> mAppPlatform;
};