#include "Precompile.h"
#include "App.h"

#include "AppPlatform.h"
#include "AppRegistration.h"
#include "component/LuaComponentRegistry.h"
#include "event/EventBuffer.h"
#include "event/LifecycleEvents.h"
#include "file/FilePath.h"
#include "file/FileSystem.h"
#include "ImGuiImpl.h"
#include "ProjectLocator.h"
#include "provider/ComponentRegistryProvider.h"
#include "provider/GameObjectHandleProvider.h"
#include "system/GraphicsSystem.h"
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
  , mAppPlatform(std::move(appPlatform))
  , mProjectLocator(std::make_unique<ProjectLocator>())
  , mMessageLock(std::make_unique<SpinLock>())
  , mGameObjectGen(std::make_unique<GameObjectGen>())
  , mComponentRegistry(Registry::createComponentRegistryProvider()) {
  FilePath path, file, ext;
  FilePath exePath(mAppPlatform->getExePath().c_str());
  exePath.getParts(path, file, ext);
  mProjectLocator->setPathRoot(path, PathSpace::Project);

  mFileSystem = mAppPlatform->createFileSystem();

  SystemArgs args = {
    mWorkerPool.get(),
    this,
    this,
    mGameObjectGen.get(),
    mProjectLocator.get(),
    mAppPlatform.get(),
    mComponentRegistry.get(),
    mFileSystem.get(),
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
  UriActivated activated(uri);
  //TODO: where should this go?
  const auto it = activated.mParams.find("projectRoot");
  if(it != activated.mParams.end() && mFileSystem->isDirectory(it->second.c_str())) {
    printf("Project root set to %s\n", it->second.c_str());
    mProjectLocator->setPathRoot(it->second.c_str(), PathSpace::Project);
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
  mMessageLock->unlock();
}

#include "imgui/imgui.h"

void App::update(float dt) {
  //Freeze message state by swapping them into freeze. Systems will look at this, while pushing to non-frozen queue
  mMessageLock->lock();
  mMessageQueue.swap(mFrozenMessageQueue);
  mMessageLock->unlock();

  ImGuiImpl::getPad().update();

  auto frameTask = std::make_shared<SyncTask>();

  for(auto& system : mSystems) {
    system->setEventBuffer(mFrozenMessageQueue.get());
    system->queueTasks(dt, *mWorkerPool, frameTask);
  }

  for(auto& system : mSystems) {
    system->update(dt, *mWorkerPool, frameTask);
  }

  mWorkerPool->queueTask(frameTask);

  if(ImGuiImpl::enabled()) {
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    static Syx::Vec3 clear_color(0.0f);
    static bool show_test_window = true;
    static bool show_another_window = true;
    ImGui::ColorEdit3("clear color", (float*)&clear_color);
    if(ImGui::Button("Test Window")) show_test_window ^= 1;
    if(ImGui::Button("Another Window")) show_another_window ^= 1;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    const int buffSize = 100;
    static char buff[buffSize] = { 0 };
    ImGui::InputText("Text In", buff, buffSize);
  }

  frameTask->sync();
  //All readers should have either looked at this in update or in a frameTask dependent task, so clear now.
  mFrozenMessageQueue->clear();
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
  return *mFileSystem;
}

MessageQueue App::getMessageQueue() {
  return MessageQueue(*mMessageQueue, *mMessageLock);
}

System* App::_getSystem(size_t id) {
  auto found = std::find_if(mSystems.begin(), mSystems.end(), [id](const std::unique_ptr<System>& system) { return system->getType() == id; });
  return found != mSystems.end() ? found->get() : nullptr;
}