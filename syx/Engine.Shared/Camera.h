#pragma once
#include <event/Event.h>

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
  bool isValid() const;

private:
  Syx::Mat4 mTransform;
  mutable Syx::Mat4 mWorldToView;
  CameraOps mOps;
  mutable bool mWorldToViewDirty;
};

class GetCameraResponse : public TypedEvent<GetCameraResponse> {
public:
  GetCameraResponse(Camera camera)
    : mCamera(std::move(camera)) {
  }

  Camera mCamera;
};

class GetCameraRequest : public RequestEvent<GetCameraRequest, GetCameraResponse> {
public:
  enum class CoordSpace : uint8_t {
    Pixel,
  };

  GetCameraRequest(const Syx::Vec2 point, CoordSpace space)
    : mPoint(point)
    , mSpace(space) {
  }

  Syx::Vec2 mPoint;
  CoordSpace mSpace;
};