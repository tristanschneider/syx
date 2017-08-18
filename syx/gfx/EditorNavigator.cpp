#include "Precompile.h"
#include "EditorNavigator.h"
#include "system/KeyboardInput.h"
#include "App.h"
#include "system/GraphicsSystem.h"
#include "Camera.h"

void EditorNavigator::update(float dt) {
  using namespace Syx;
  const KeyboardInput& in = mApp->getSystem<KeyboardInput>(SystemId::KeyboardInput);
  GraphicsSystem& graphics = mApp->getSystem<GraphicsSystem>(SystemId::Graphics);
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
  move.SafeNormalize();
  camPos += move*dt*speed;

  bool rotated = false;
  if(in.getKeyDown(Key::RightMouse)) {
    float sensitivity = 0.01f;
    Vec2 rot = -in.getMouseDelta()*sensitivity;
    Mat3 yRot = Mat3::YRot(rot.x);
    Mat3 xRot = Mat3::AxisAngle(camRot.GetCol(0), rot.y);
    camRot = yRot * xRot * camRot;
    rotated = true;
  }

  if(move.Length2() > 0.0f || rotated) {
    cam.setTransform(Mat4::transform(camRot, camPos));
  }
}