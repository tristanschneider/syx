#pragma once
#include "system/System.h"
#include "provider/MessageQueueProvider.h"
#include "provider/SystemProvider.h"

class AppRegistration;
class ComponentRegistryProvider;
class GameObjectHandleProvider;
class GraphicsSystem;
class KeyboardInput;
class System;
enum class SystemId : uint8_t;
struct IIDRegistry;
class IWorkerPool;
class AppPlatform;
class ProjectLocator;
class SpinLock;

namespace FileSystem {
  struct IFileSystem;
}

class App
  : public MessageQueueProvider
  , public SystemProvider {
public:
  App(std::unique_ptr<AppPlatform> appPlatform, std::unique_ptr<AppRegistration> registration);
  ~App();

  void onUriActivated(std::string uri);

  void init();
  void update(float dt);
  void uninit();
  IWorkerPool& getWorkerPool();
  AppPlatform& getAppPlatform();
  FileSystem::IFileSystem& getFileSystem();

  MessageQueue getMessageQueue() override;
  DeferredMessageQueue getDeferredMessageQueue() override;
  System* _getSystem(size_t id) override;

private:
  std::vector<std::unique_ptr<System>> mSystems;
  std::unique_ptr<IWorkerPool> mWorkerPool;
  std::unique_ptr<AppPlatform> mAppPlatform;
  //Message queue is what is pushed to every frame, frozen is what all systems look at each frame to read from
  std::unique_ptr<EventBuffer> mMessageQueue;
  std::unique_ptr<EventBuffer> mFrozenMessageQueue;
  std::unique_ptr<DeferredEventBuffer> mDeferredEventBuffer;
  std::mutex mDeferredEventMutex;
  std::unique_ptr<ProjectLocator> mProjectLocator;
  std::unique_ptr<GameObjectHandleProvider> mGameObjectGen;
  std::unique_ptr<SpinLock> mMessageLock;
  std::unique_ptr<ComponentRegistryProvider> mComponentRegistry;
  std::unique_ptr<FileSystem::IFileSystem> mFileSystem;
  std::unique_ptr<IIDRegistry> mIDRegistry;
  uint64_t mCurrentTick = 0;
};