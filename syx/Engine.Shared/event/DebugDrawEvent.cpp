#include "Precompile.h"
#include "event/DebugDrawEvent.h"

DEFINE_EVENT(DrawLineEvent, const Syx::Vec3& start, const Syx::Vec3& end, const Syx::Vec3& color)
  , mStart(start)
  , mEnd(end)
  , mColor(color) {
}

DEFINE_EVENT(DrawVectorEvent, const Syx::Vec3& start, const Syx::Vec3& dir, const Syx::Vec3& color)
  , mStart(start)
  , mDir(dir)
  , mColor(color) {
}

DEFINE_EVENT(DrawPointEvent, const Syx::Vec3& point, float size, const Syx::Vec3& color)
  , mPoint(point)
  , mSize(size)
  , mColor(color) {
}

DEFINE_EVENT(DrawCubeEvent, const Syx::Vec3& center, const Syx::Vec3& size, const Syx::Quat& rot, const Syx::Vec3& color)
  , mCenter(center)
  , mSize(size)
  , mRot(rot)
  , mColor(color) {
}

DEFINE_EVENT(DrawSphereEvent, const Syx::Vec3& center, float radius, const Syx::Quat& rot, const Syx::Vec3& color)
  , mCenter(center)
  , mRadius(radius)
  , mRot(rot)
  , mColor(color) {
}
