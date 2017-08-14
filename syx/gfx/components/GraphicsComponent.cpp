#include "Precompile.h"
#include "GraphicsComponent.h"

GraphicsComponent::GraphicsComponent(Handle owner)
  : Component(owner)
  , mModel(InvalidHandle)
  , mDiffTex(InvalidHandle) {
}