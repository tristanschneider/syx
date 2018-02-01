#pragma once
#include "system/System.h"
#include "MessageQueueProvider.h"
#include "SystemProvider.h"

class GraphicsSystem;
class KeyboardInput;
class System;
class Space;
enum class SystemId : uint8_t;
class IWorkerPool;
class AppPlatform;

class App
  : public MessageQueueProvider
  , public SystemProvider {
public:
  App(std::unique_ptr<AppPlatform> appPlatform);
  ~App();

  void init();
  void update(float dt);
  void uninit();
  Space& getDefaultSpace();
  IWorkerPool& getWorkerPool();
  AppPlatform& getAppPlatform();

  MessageQueue getMessageQueue() override;
  System* _getSystem(size_t id) override;

  //Temporary until asset manager that wraps asset loading and such
  std::unordered_map<std::string, Handle> mAssets;

private:
  std::vector<std::unique_ptr<System>> mSystems;
  std::unique_ptr<Space> mDefaultSpace;
  std::unique_ptr<IWorkerPool> mWorkerPool;
  std::unique_ptr<AppPlatform> mAppPlatform;
  std::unique_ptr<EventListener> mMessageQueue;
  SpinLock mMessageLock;
};