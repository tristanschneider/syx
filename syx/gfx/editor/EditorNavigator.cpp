#include "Precompile.h"
#include "editor/EditorNavigator.h"
#include "system/KeyboardInput.h"
#include "system/GraphicsSystem.h"
#include "provider/SystemProvider.h"
#include "Camera.h"

RegisterSystemCPP(EditorNavigator);

void EditorNavigator::update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
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