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

void Viewport::set(const Syx::Vec2& min, const Syx::Vec2& max) {
  mMin = min;
  mMax = max;
}