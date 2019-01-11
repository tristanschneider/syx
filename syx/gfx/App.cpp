#include "Precompile.h"
#include "App.h"

#include "AppPlatform.h"
#include "event/EventBuffer.h"
#include "event/LifecycleEvents.h"
#include "file/FilePath.h"
#include "ImGuiImpl.h"
#include "ProjectLocator.h"
#include "system/GraphicsSystem.h"
#include "test/TestRegistry.h"
#include "threading/WorkerPool.h"
#include "threading/SyncTask.h"
#include "util/ScratchPad.h"

App::App(std::unique_ptr<AppPlatform> appPlatform)
  : mWorkerPool(std::make_unique<WorkerPool>(4))
  , mMessageQueue(std::make_unique<EventBuffer>())
  , mFrozenMessageQueue(std::make_unique<EventBuffer>())
  , mAppPlatform(std::move(appPlatform))
  , mProjectLocator(std::make_unique<ProjectLocator>()) {
  FilePath path, file, ext;
  FilePath exePath(mAppPlatform->getExePath().c_str());
  exePath.getParts(path, file, ext);
  mProjectLocator->setPathRoot(path, PathSpace::Project);
  SystemArgs args = {
    mWorkerPool.get(),
    this,
    this,
    this,
    mProjectLocator.get()
  };
  System::Registry::getSystems(args, mSystems);

  TestRegistry::get().run();
}

App::~App() {
  mSystems.clear();
  mMessageQueue = nullptr;
}

class AppFocusListener : public FocusEvents {
  void onFocusGained() override {
    printf("focus gained\n");
  }
  void onFocusLost() override {
    printf("focus lost\n");
  }
};

class AppDirectoryWatcher : public DirectoryWatcher {
  void onFileChanged(const std::string& filename) override {
    printf("File changed: %s\n", filename.c_str());
  }
  void onFileAdded(const std::string& filename) {
    printf("File added: %s\n", filename.c_str());
  }
  void onFileRemoved(const std::string& filename) {
    printf("File removed: %s\n", filename.c_str());
  }
  void onFileRenamed(const std::string& oldName, const std::string& newName) {
    printf("File renamed from: %s to %s\n", oldName.c_str(), newName.c_str());
  }
};

void App::onUriActivated(std::string uri) {
  mMessageQueue->push(UriActivated(uri));
}

void App::init() {
  for(auto& system : mSystems) {
    if(system)
      system->init();
  }
  mMessageLock.lock();
  mMessageQueue->push(AllSystemsInitialized());
  mMessageLock.unlock();

  static FocusEvents::ObserverType o(std::make_unique<AppFocusListener>());
  if(!o.hasSubject())
    getAppPlatform().addFocusObserver(o);
  static DirectoryWatcher::ObserverType d(std::make_unique<AppDirectoryWatcher>());
  if(!d.hasSubject())
    getAppPlatform().addDirectoryObserver(d);
}

#include "imgui/imgui.h"

void App::update(float dt) {
  //Freeze message state by swapping them into freeze. Systems will look at this, while pushing to non-frozen queue
  mMessageLock.lock();
  mMessageQueue.swap(mFrozenMessageQueue);
  mMessageLock.unlock();

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

MessageQueue App::getMessageQueue() {
  return MessageQueue(*mMessageQueue, mMessageLock);
}

System* App::_getSystem(size_t id) {
  return id < mSystems.size() ? mSystems[id].get() : nullptr;
}

Handle App::newHandle() {
  return mGameObjectGen.next();
}
