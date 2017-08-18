#include "Precompile.h"
#include "Renderable.h"

Renderable::Renderable(Handle owner)
  : Component(owner)
  , mModel(InvalidHandle)
  , mDiffTex(InvalidHandle) {
}