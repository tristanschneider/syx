#include "Precompile.h"
#include "editor/Editor.h"

#include "AppPlatform.h"
#include "Camera.h"
#include "component/CameraComponent.h"
#include "component/SpaceComponent.h"
#include "editor/AssetPreview.h"
#include "editor/ObjectInspector.h"
#include "editor/SceneBrowser.h"
#include "editor/Toolbox.h"
#include "event/BaseComponentEvents.h"
#include "event/EditorEvents.h"
#include <event/EventBuffer.h>
#include <event/EventHandler.h>
#include "event/LifecycleEvents.h"
#include "event/SpaceEvents.h"
#include "event/ViewportEvents.h"
#include "file/FilePath.h"
#include "file/FileSystem.h"
#include "LuaGameObject.h"
#include "ProjectLocator.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/SystemProvider.h"
#include "system/AssetRepo.h"
#include "system/KeyboardInput.h"
#include "system/LuaGameSystem.h"
#include "system/GraphicsSystem.h"
#include <threading/FunctionTask.h>
#include <threading/IWorkerPool.h>

RegisterSystemCPP(Editor);

class EditorGameObserver : public LuaGameSystemObserver {
public:
  EditorGameObserver(std::function<void()> update)
    : mUpdate(std::move(update)) {
  }

  virtual ~EditorGameObserver() {}
  void preUpdate(const LuaGameSystem& game) override {
    mUpdate();
  }

private:
  std::function<void()> mUpdate;
};

class DragDropAssetLoader : public DragDropObserver {
public:
  DragDropAssetLoader(AssetRepo& repo, IWorkerPool& workerPool, const ProjectLocator& locator)
    : mAssets(repo)
    , mPool(workerPool)
    , mLocator(locator) {
  }

  void onDrop(const std::vector<FilePath>& files) override {
    std::vector<FilePath> myFiles = files;
    mPool.queueTask(std::make_shared<FunctionTask>([this, myFiles = std::move(files)]() mutable {
      _expandDirectories(myFiles);
      _loadAssets(myFiles);
    }));
  }

private:
  void _expandDirectories(std::vector<FilePath>& files) {
    size_t end = files.size();
    for(size_t i = 0; i < end; ++i) {
      const FilePath file = files[i];
      if(FileSystem::isDirectory(file)) {
        FileSystem::forEachInDirectoryRecursive(files[i].cstr(), [&files](std::string_view file) {
          files.emplace_back(file.data());
        });
      }
    }
  }

  void _loadAssets(const std::vector<FilePath>& files) {
    for(const FilePath& file : files) {
      const FilePath relativePath = mLocator.transform(file.cstr(), PathSpace::Full, PathSpace::Project);
      //Skip paths that aren't under the project
      if(relativePath != file) {
        if(mAssets.getAsset(AssetInfo(relativePath.cstr()))) {
          printf("Loading asset %s\n", relativePath.cstr());
        }
        else {
          printf("Unsupported asset type %s\n", relativePath.cstr());
        }
      }
      else {
        printf("Ignoring asset not under project root %s\n", file.cstr());
      }
    }
  }

  AssetRepo& mAssets;
  IWorkerPool& mPool;
  const ProjectLocator& mLocator;
};

void Editor::init() {
  mCurrentState = PlayState::Stopped;
  mEventHandler = std::make_unique<EventHandler>();

  mSavedScene = std::make_unique<FilePath>(mArgs.mProjectLocator->transform("scene.json", PathSpace::Project, PathSpace::Full));

  mSceneBrowser = std::make_unique<SceneBrowser>(*mArgs.mMessages, *mArgs.mGameObjectGen, *mArgs.mSystems->getSystem<KeyboardInput>(), *mEventHandler);
  mObjectInspector = std::make_unique<ObjectInspector>(*mArgs.mMessages, *mEventHandler, *mArgs.mSystems->getSystem<LuaGameSystem>());
  mAssetPreview = std::make_unique<AssetPreview>(*mArgs.mMessages, *mEventHandler, *mArgs.mSystems->getSystem<AssetRepo>());
  mToolbox = std::make_unique<Toolbox>(*mArgs.mMessages, *mEventHandler);
  mDragDropAssetLoader = std::make_unique<DragDropAssetLoader>(*mArgs.mSystems->getSystem<AssetRepo>(), *mArgs.mPool, *mArgs.mProjectLocator);
  mArgs.mAppPlatform->addDragDropObserver(*mDragDropAssetLoader);

  mEventHandler->registerEventHandler<AllSystemsInitialized>([this](const AllSystemsInitialized&) {
    mGameObserver = std::make_unique<EditorGameObserver>(std::bind(&Editor::_editorUpdate, this));
    mArgs.mSystems->getSystem<LuaGameSystem>()->addObserver(*mGameObserver);
  });

  mEventHandler->registerEventHandler<UriActivated>([this](const UriActivated& e) {
    const auto it = e.mParams.find("loadScene");
    if(it != e.mParams.end() && FileSystem::fileExists(it->second.c_str())) {
      //TODO: should this path be kept seperate from mSavedScene?
      *mSavedScene = FilePath(it->second.c_str());
      MessageQueue msg = mArgs.mMessages->getMessageQueue();
      msg.get().push(ClearSpaceEvent(_getEditorSpace()));
      msg.get().push(LoadSpaceEvent(_getEditorSpace(), *mSavedScene));
    }
  });

  mEventHandler->registerEventHandler<SetPlayStateEvent>([this](const SetPlayStateEvent& e) {
    _updateState(e.mState);
  });
  mEventHandler->registerEventHandler<CallbackEvent>(CallbackEvent::getHandler(GetSystemID(Editor)));

  //TODO: make this less confusing and error prone
  MessageQueue msg = mArgs.mMessages->getMessageQueue();
  msg.get().push(SetViewportEvent(Viewport("editor", Syx::Vec2::sZero, Syx::Vec2::sIdentity)));
  mCamera = std::make_unique<LuaGameObject>(mArgs.mGameObjectGen->newHandle());
  msg.get().push(AddGameObjectEvent(mCamera->getHandle()));
  mCamera->getComponent<SpaceComponent>()->set(std::hash<std::string>()("editor"));
  mCamera->getComponent<SpaceComponent>()->sync(msg.get());

  auto cc = std::make_unique<CameraComponent>(mCamera->getHandle());
  cc->setViewport("editor");
  cc->addSync(msg.get());
  mCamera->addComponent(std::move(cc));
}

void Editor::uninit() {
  mEventHandler = nullptr;
}

void Editor::update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  mEventHandler->handleEvents(*mEventBuffer);

  _updateInput(dt);
}

void Editor::_editorUpdate() {
  const LuaGameSystem& game = *mArgs.mSystems->getSystem<LuaGameSystem>();
  mSceneBrowser->editorUpdate(game.getObjects());
  mObjectInspector->editorUpdate(game);
  mAssetPreview->editorUpdate();
  mToolbox->editorUpdate(*mArgs.mSystems->getSystem<KeyboardInput>());

  Component::EditorUpdateArgs args{ game,
    mArgs.mSystems->getSystem<GraphicsSystem>()->getDebugDrawer(),
    *mArgs.mMessages,
    *mCamera,
  };
  for(const auto& obj : game.getObjects()) {
    const bool selected = mObjectInspector->isSelected(obj.second->getHandle());
    obj.second->forEachComponent([this, &obj, selected, &args](const Component& comp) {
      comp.onEditorUpdate(*obj.second, selected, args);
    });
  }
}

void Editor::_updateState(PlayState state) {
  if(mCurrentState == state)
    return;

  const PlayState prevState = mCurrentState;
  mCurrentState = state;
  const bool startedPlaying = prevState == PlayState::Stopped && mCurrentState == PlayState::Playing;
  const bool stoppedPlaying = mCurrentState == PlayState::Stopped;
  if(startedPlaying) {
    MessageQueue msg = mArgs.mMessages->getMessageQueue();
    msg.get().push(SaveSpaceEvent(_getEditorSpace(), *mSavedScene));
    msg.get().push(ClearSpaceEvent(_getPlaySpace()));
    msg.get().push(LoadSpaceEvent(_getPlaySpace(), *mSavedScene));
  }
  else if(stoppedPlaying) {
    MessageQueue msg = mArgs.mMessages->getMessageQueue();
    msg.get().push(ClearSpaceEvent(_getEditorSpace()));
    msg.get().push(LoadSpaceEvent(_getEditorSpace(), *mSavedScene));
  }

  const float timescale = [](PlayState state) {
    switch(state) {
      case PlayState::Paused:
      case PlayState::Stopped:
        return 0.0f;
      case PlayState::Playing:
      case PlayState::Stepping:
      default:
        return 1.0f;
    }
  }(mCurrentState);
  mArgs.mMessages->getMessageQueue().get().push(SetTimescaleEvent(_getEditorSpace(), timescale));
}

Handle Editor::_getEditorSpace() const {
  //Everything is using the default scene for simplicity
  return 0;
}

Handle Editor::_getPlaySpace() const {
  return 0;
}

void Editor::_updateInput(float dt) {
  using namespace Syx;
  const KeyboardInput& in = *mArgs.mSystems->getSystem<KeyboardInput>();
  GraphicsSystem& graphics = *mArgs.mSystems->getSystem<GraphicsSystem>();
  Vec3 move = Vec3::Zero;
  Mat4 camTransform = mCamera->getComponent<Transform>()->get();
  Vec3 camPos;
  Mat3 camRot;
  camTransform.decompose(camRot, camPos);

  if(in.getKeyDown(Key::KeyW))
    move.z -= 1.0f;
  if(in.getKeyDown(Key::KeyS))
    move.z += 1.0f;
  if(in.getKeyDown(Key::KeyA))
    move.x -= 1.0f;
  if(in.getKeyDown(Key::KeyD))
    move.x += 1.0f;

  float speedMod = 3.0f;
  if(in.getKeyDown(Key::Shift))
    speedMod *= 3.0f;

  float vertical = 0.0f;
  if(in.getKeyDown(Key::Space))
    vertical += 1.0f;
  if(in.getKeyDown(Key::KeyC))
    vertical -= 1.0f;

  float speed = 3.0f*speedMod;
  move = camRot * move;
  move.y += vertical;
  move.safeNormalize();
  camPos += move*dt*speed;

  bool rotated = false;
  if(in.getKeyDown(Key::RightMouse)) {
    float sensitivity = 0.01f;
    Vec2 rot = -in.getMouseDelta()*sensitivity;
    Mat3 yRot = Mat3::yRot(rot.x);
    Mat3 xRot = Mat3::axisAngle(camRot.getCol(0), rot.y);
    camRot = yRot * xRot * camRot;
    rotated = true;
  }

  if(move.length2() > 0.0f || rotated) {
    mCamera->getComponent<Transform>()->set(Mat4::transform(camRot, camPos));
    mCamera->getComponent<Transform>()->sync(mArgs.mMessages->getMessageQueue().get());
  }
}