#include "Precompile.h"
#include "Renderable.h"

Renderable::Renderable(Handle owner, MessagingSystem& messaging)
  : Component(owner, &messaging)
  , mModel(InvalidHandle)
  , mDiffTex(InvalidHandle) {
}