#include "Precompile.h"
#include "TransformComponent.h"

using namespace Syx;

TransformComponent::TransformComponent(Handle owner) 
  : Component(owner)
  , mMat(Mat4::transform(Quat::Identity, Vec3::Zero)) {
}
