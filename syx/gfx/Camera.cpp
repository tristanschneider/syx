#include "Precompile.h"
#include "Camera.h"

Camera::Camera(const CameraOps& ops)
  : mWorldToViewDirty(true)
  , mTransform(Syx::Mat4::transform(Syx::Vec3::Identity, Syx::Quat::lookAt(Syx::Vec3::UnitZ), Syx::Vec3::Zero))
  , mOps(ops) {
}

const Syx::Mat4& Camera::getTransform() const {
  return mTransform;
}

void Camera::setTransform(const Syx::Mat4& t) {
  mTransform = t;
  mWorldToViewDirty = true;
}

const Syx::Mat4& Camera::getWorldToView() const {
  if(mWorldToViewDirty) {
    Syx::Mat4 view = Syx::Mat4::perspective(mOps.mFOVX, mOps.mFOVY, mOps.mNear, mOps.mFar);
    mWorldToView = view * mTransform.affineInverse();
    mWorldToViewDirty = false;
  }
  return mWorldToView;
}

void Camera::setOps(const CameraOps& ops) {
  mOps = ops;
  mWorldToViewDirty = true;
}

const CameraOps& Camera::getOps() const {
  return mOps;
}

void Camera::setViewport(const std::string& viewport) {
  mOps.mViewport = viewport;
}
