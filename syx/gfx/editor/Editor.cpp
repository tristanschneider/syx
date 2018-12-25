#include "Precompile.h"
#include "editor/Editor.h"

#include "editor/AssetPreview.h"
#include "editor/ObjectInspector.h"
#include "editor/SceneBrowser.h"
#include "editor/Toolbox.h"
#include "event/EditorEvents.h"
#include <event/EventHandler.h>
#include "event/LifecycleEvents.h"
#include "system/AssetRepo.h"
#include "system/KeyboardInput.h"
#include "system/LuaGameSystem.h"
#include "system/GraphicsSystem.h"
#include "provider/SystemProvider.h"
#include "Camera.h"

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

void Editor::init() {
  mEventHandler = std::make_unique<EventHandler>();

  mSceneBrowser = std::make_unique<SceneBrowser>(*mArgs.mMessages, *mArgs.mGameObjectGen, *mArgs.mSystems->getSystem<KeyboardInput>(), *mEventHandler);
  mObjectInspector = std::make_unique<ObjectInspector>(*mArgs.mMessages, *mEventHandler);
  mAssetPreview = std::make_unique<AssetPreview>(*mArgs.mMessages, *mEventHandler, *mArgs.mSystems->getSystem<AssetRepo>());
  mToolbox = std::make_unique<Toolbox>(*mArgs.mMessages, *mEventHandler);

  mEventHandler->registerEventHandler<AllSystemsInitialized>([this](const AllSystemsInitialized&) {
    mGameObserver = std::make_unique<LuaGameSystemObserverT>(std::make_unique<EditorGameObserver>(std::bind(&Editor::_editorUpdate, this)));
    mArgs.mSystems->getSystem<LuaGameSystem>()->addObserver(*mGameObserver);
  });
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
}

void Editor::_updateInput(float dt) {
  using namespace Syx;
  const KeyboardInput& in = *mArgs.mSystems->getSystem<KeyboardInput>();
  GraphicsSystem& graphics = *mArgs.mSystems->getSystem<GraphicsSystem>();
  Vec3 move = Vec3::Zero;
  Camera& cam = graphics.getPrimaryCamera();
  Mat4 camTransform = cam.getTransform();
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
    cam.setTransform(Mat4::transform(camRot, camPos));
  }
}