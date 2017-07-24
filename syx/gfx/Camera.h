#pragma once

struct CameraOps {
  CameraOps(float fovx, float fovy, float nearPlane, float farPlane)
    : mFOVX(fovx)
    , mFOVY(fovy)
    , mNear(nearPlane)
    , mFar(farPlane) {
  }

  float mFOVX;
  float mFOVY;
  float mNear;
  float mFar;
};

class Camera {
public:
  Camera(const CameraOps& ops);

  const Syx::Mat4& getTransform() const;
  void setTransform(const Syx::Mat4& t);

  const Syx::Mat4& getWorldToView();

  void setOps(const CameraOps& ops);
  const CameraOps& getOps();


private:
  Syx::Mat4 mTransform;
  Syx::Mat4 mWorldToView;
  CameraOps mOps;
  bool mWorldToViewDirty;
};