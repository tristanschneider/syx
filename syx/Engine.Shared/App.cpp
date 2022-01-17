#include "Precompile.h"
#include "App.h"

#include "AppPlatform.h"
#include "AppRegistration.h"
#include "component/LuaComponentRegistry.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/MessageComponent.h"
#include "ecs/component/ProjectLocatorComponent.h"
#include "ecs/component/UriActivationComponent.h"
#include "event/DeferredEventBuffer.h"
#include "event/EventBuffer.h"
#include "event/LifecycleEvents.h"
#include "file/FilePath.h"
#include "file/FileSystem.h"
#include "ImGuiImpl.h"
#include "ProjectLocator.h"
#include "provider/ComponentRegistryProvider.h"
#include "provider/GameObjectHandleProvider.h"
#include "registry/IDRegistry.h"
#include "test/TestRegistry.h"
#include "threading/FunctionTask.h"
#include "threading/SpinLock.h"
#include "threading/SyncTask.h"
#include "threading/WorkerPool.h"
#include "util/ScratchPad.h"

class GameObjectGen : public GameObjectHandleProvider {
public:
  Handle newHandle() override {
    return mGen.next();
  }

  bool blacklistHandle(Handle used) override {
    return mGen.blacklistHandle(used);
  }

private:
  HandleGen mGen;
};

App::App(std::unique_ptr<AppPlatform> appPlatform, std::unique_ptr<AppRegistration> registration)
  : mWorkerPool(std::make_unique<WorkerPool>(4))
  , mMessageQueue(std::make_unique<EventBuffer>())
  , mFrozenMessageQueue(std::make_unique<EventBuffer>())
  , mDeferredEventBuffer(std::make_unique<DeferredEventBuffer>())
  , mAppPlatform(std::move(appPlatform))
  , mMessageLock(std::make_unique<SpinLock>())
  , mGameObjectGen(std::make_unique<GameObjectGen>())
  , mComponentRegistry(Registry::createComponentRegistryProvider())
  , mIDRegistry(create::idRegistry()) {
  FilePath path, file, ext;
  FilePath exePath(mAppPlatform->getExePath().c_str());
  exePath.getParts(path, file, ext);

  auto fileSystem = mAppPlatform->createFileSystem();

  ecx::SchedulerConfig config;
  mScheduler = std::make_shared<Engine::Scheduler>(config);
  mAppContext = std::make_unique<Engine::AppContext>(mScheduler);
  mEntityRegistry = std::make_unique<Engine::EntityRegistry>();
  registration->registerAppContext(*mAppContext);

  mAppContext->initialize(*mEntityRegistry);
  auto projectLocator = mEntityRegistry->begin<ProjectLocatorComponent>();
  assert(projectLocator != mEntityRegistry->end<ProjectLocatorComponent>() && "Should have been added by AppContext::initialize");
  projectLocator->get().setPathRoot(path, PathSpace::Project);

  auto globalEntity = mEntityRegistry->getSingleton();
  mEntityRegistry->addComponent<FileSystemComponent>(globalEntity, std::move(fileSystem));

  SystemArgs args = {
    mWorkerPool.get(),
    this,
    mGameObjectGen.get(),
    &projectLocator->get(),
    mAppPlatform.get(),
    mComponentRegistry.get(),
    fileSystem.get(),
    mIDRegistry.get(),
  };
  auto systems = Registry::createSystemRegistry();
  registration->registerSystems(args, *systems);
  registration->registerComponents(mComponentRegistry->getWriter().first);
  mSystems = systems->takeSystems();

  //TODO: move this to test project
  TestRegistry::get().run();
}

App::~App() {
  mSystems.clear();
  mMessageQueue = nullptr;
}

void App::onUriActivated(std::string uri) {
  auto message = mEntityRegistry->createEntity();
  mEntityRegistry->addComponent<MessageComponent>(message);
  mEntityRegistry->addComponent<UriActivationComponent>(message, UriActivationComponent{ uri });

  //TODO: move all handlers of this to ecs
  UriActivated activated(uri);

  //TODO: This is already handled by ProjectLocatorSystem but can't be removed until UriActivated usages are migrated due to timing requirements
  const auto it = activated.mParams.find("projectRoot");
  if(it != activated.mParams.end() && getFileSystem().isDirectory(it->second.c_str())) {
    printf("Project root set to %s\n", it->second.c_str());
    mEntityRegistry->begin<ProjectLocatorComponent>()->get().setPathRoot(it->second.c_str(), PathSpace::Project);
    mAppPlatform->setWorkingDirectory(it->second.c_str());
  }

  mMessageQueue->push(std::move(activated));
}

void App::init() {
  for(auto& system : mSystems) {
    if(system)
      system->init();
  }
  mMessageLock->lock();
  mMessageQueue->push(AllSystemsInitialized());
  mMessageQueue->push(FrameStart());
  mMessageLock->unlock();
}

void App::update(float dt) {
  //Freeze message state by swapping them into freeze. Systems will look at this, while pushing to non-frozen queue
  mMessageLock->lock();
  //Push an event for the end of the last frame
  mMessageQueue->push(FrameEnd());
  mMessageQueue.swap(mFrozenMessageQueue);
  //The empty frozen queue just got swapped into mMessageQueue, push the first event: FrameStart
  mMessageQueue->push(FrameStart());
  mMessageLock->unlock();

  auto frameTask = std::make_shared<SyncTask>();

  //Must be done separately incase queueTasks causes cross-system communication
  for(auto& system : mSystems) {
    system->setEventBuffer(mFrozenMessageQueue.get());
  }

  for(auto& system : mSystems) {
    system->queueTasks(dt, *mWorkerPool, frameTask);
  }

  for(auto& system : mSystems) {
    system->update(dt, *mWorkerPool, frameTask);
  }

  mWorkerPool->queueTask(frameTask);

  //There is no guarantee of the timing of deferred events, so this can go anywhere. It is here since the only other thing this thread would do is wait on the sync
  {
    //The frozen swap as done for normal events would be more efficient. Currently the assumption is that deffered event volume is dramatically lower than normal events so it's not worth the added complexity
    auto deferredMsg = getDeferredMessageQueue();
    auto msg = getMessageQueue();
    deferredMsg->enqueueDeferredEvents(*msg, mCurrentTick);
  }

  mAppContext->update(*mEntityRegistry);

  frameTask->sync();
  //All readers should have either looked at this in update or in a frameTask dependent task, so clear now.
  mFrozenMessageQueue->clear();
  ++mCurrentTick;
}

void App::uninit() {
  for(auto& system : mSystems) {
    if(system)
      system->uninit();
  }
}

IWorkerPool& App::getWorkerPool() {
  return *mWorkerPool;
}

AppPlatform& App::getAppPlatform() {
  return *mAppPlatform;
}

FileSystem::IFileSystem& App::getFileSystem() {
  return mEntityRegistry->begin<FileSystemComponent>()->get();
}

MessageQueue App::getMessageQueue() {
  return MessageQueue(*mMessageQueue, *mMessageLock);
}

DeferredMessageQueue App::getDeferredMessageQueue() {
  return DeferredMessageQueue(*mDeferredEventBuffer, mDeferredEventMutex);
}

System* App::_getSystem(typeId_t<System> id) {
  auto found = std::find_if(mSystems.begin(), mSystems.end(), [id](const std::shared_ptr<System>& system) { return system->getType() == id; });
  return found != mSystems.end() ? found->get() : nullptr;
}