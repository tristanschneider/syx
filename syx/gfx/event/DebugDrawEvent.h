#pragma once
#include "event/Event.h"

class DrawLineEvent : public Event {
public:
  DrawLineEvent(const Syx::Vec3& start, const Syx::Vec3& end, const Syx::Vec3& color);

  Syx::Vec3 mStart;
  Syx::Vec3 mEnd;
  Syx::Vec3 mColor;
};

class DrawVectorEvent : public Event {
public:
  DrawVectorEvent(const Syx::Vec3& start, const Syx::Vec3& dir, const Syx::Vec3& color);

  Syx::Vec3 mStart;
  Syx::Vec3 mDir;
  Syx::Vec3 mColor;
};

class DrawPointEvent : public Event {
public:
  DrawPointEvent(const Syx::Vec3& point, float size, const Syx::Vec3& color);

  Syx::Vec3 mPoint;
  float mSize;
  Syx::Vec3 mColor;
};

class DrawCubeEvent : public Event {
public:
  DrawCubeEvent(const Syx::Vec3& center, const Syx::Vec3& size, const Syx::Quat& rot, const Syx::Vec3& color);

  Syx::Vec3 mCenter;
  Syx::Vec3 mSize;
  Syx::Quat mRot;
  Syx::Vec3 mColor;
};

class DrawSphereEvent : public Event {
public:
  DrawSphereEvent(const Syx::Vec3& center, float radius, const Syx::Quat& rot, const Syx::Vec3& color);

  Syx::Vec3 mCenter;
  float mRadius;
  Syx::Quat mRot;
  Syx::Vec3 mColor;
};