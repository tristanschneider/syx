#pragma once
#include "system/System.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/MessageQueueProvider.h"
#include "provider/SystemProvider.h"

class GraphicsSystem;
class KeyboardInput;
class System;
enum class SystemId : uint8_t;
class IWorkerPool;
class AppPlatform;
class ProjectLocator;

class App
  : public MessageQueueProvider
  , public SystemProvider
  , public GameObjectHandleProvider {
public:
  App(std::unique_ptr<AppPlatform> appPlatform);
  ~App();

  void init();
  void update(float dt);
  void uninit();
  IWorkerPool& getWorkerPool();
  AppPlatform& getAppPlatform();

  MessageQueue getMessageQueue() override;
  System* _getSystem(size_t id) override;
  //GameObjectHandleProvider
  Handle newHandle() override;

private:
  std::vector<std::unique_ptr<System>> mSystems;
  std::unique_ptr<IWorkerPool> mWorkerPool;
  std::unique_ptr<AppPlatform> mAppPlatform;
  //Message queue is what is pushed to every frame, frozen is what all systems look at each frame to read from
  std::unique_ptr<EventBuffer> mMessageQueue;
  std::unique_ptr<EventBuffer> mFrozenMessageQueue;
  std::unique_ptr<ProjectLocator> mProjectLocator;
  SpinLock mMessageLock;
  HandleGen mGameObjectGen;
};