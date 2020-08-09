#include "Precompile.h"
#include "Space.h"

Space::Space(Handle id)
  : mInstance(0)
  , mTimescale(0.0f) {
  mInstance.set(id);
}

float Space::getTimescale() const {
  return mTimescale;
}

void Space::setTimescale(float timescale) {
  mTimescale = timescale;
}

int Space::push(lua_State* l) const {
  return mInstance.push(l);
}
