#include "Precompile.h"
#include "Transform.h"

using namespace Syx;

Transform::Transform(Handle owner) 
  : Component(owner)
  , mMat(Mat4::transform(Quat::Identity, Vec3::Zero)) {
}
