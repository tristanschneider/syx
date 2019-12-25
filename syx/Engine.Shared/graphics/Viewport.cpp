#include "Precompile.h"
#include "Viewport.h"

Viewport::Viewport(std::string name, const Syx::Vec2& min, const Syx::Vec2& max)
  : mName(std::move(name))
  , mMin(min)
  , mMax(max) {
}

const std::string& Viewport::getName() const {
  return mName;
}

Syx::Vec2 Viewport::getMin() const {
  return mMin;
}

Syx::Vec2 Viewport::getMax() const {
  return mMax;
}

bool Viewport::within(const Syx::Vec2& point) const {
  return point.x >= mMin.x &&
    point.x <= mMax.x &&
    point.y >= mMin.y &&
    point.y <= mMax.y;
}

void Viewport::set(const Syx::Vec2& min, const Syx::Vec2& max) {
  mMin = min;
  mMax = max;
}