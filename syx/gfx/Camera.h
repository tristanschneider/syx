#pragma once

struct CameraOps {
  CameraOps(float fovx, float fovy, float nearPlane, float farPlane, Handle owner = InvalidHandle)
    : mFOVX(fovx)
    , mFOVY(fovy)
    , mNear(nearPlane)
    , mFar(farPlane)
    , mOwner(owner) {
  }

  Handle mOwner;
  float mFOVX;
  float mFOVY;
  float mNear;
  float mFar;
  std::string mViewport;
};

class Camera {
public:
  Camera(const CameraOps& ops);

  const Syx::Mat4& getTransform() const;
  void setTransform(const Syx::Mat4& t);

  const Syx::Mat4& getWorldToView() const;

  void setOps(const CameraOps& ops);
  const CameraOps& getOps() const;

  void setViewport(const std::string& viewport);

private:
  Syx::Mat4 mTransform;
  mutable Syx::Mat4 mWorldToView;
  CameraOps mOps;
  mutable bool mWorldToViewDirty;
};