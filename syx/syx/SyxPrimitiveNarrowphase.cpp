#include "Precompile.h"
#include "SyxPrimitiveNarrowphase.h"
#include "SyxNarrowphase.h"
#include "SyxModelInstance.h"

namespace Syx {
  void PrimitiveNarrowphase::Set(ModelInstance* a, ModelInstance* b, Space* space, Narrowphase* narrowphase) {
    mA = a;
    mB = b;
    mSpace = space;
    mNarrowphase = narrowphase;
  }

  void PrimitiveNarrowphase::SphereSphere(void) {
    const Transformer& ta = mA->GetModelToWorld();
    const Transformer& tb = mB->GetModelToWorld();
    const Vec3& posA = ta.mPos;
    const Vec3& posB = tb.mPos;
    float radiusA = ta.mScaleRot.mbx.Length();
    float radiusB = tb.mScaleRot.mbx.Length();

    Vec3 aToB = posB - posA;
    float dist = aToB.Length2();
    float combinedRadius = radiusA + radiusB;
    if(dist > combinedRadius*combinedRadius)
      return;

    Vec3 normalA;
    //If dist is zero then all normals are equally valid, so arbitrarily pick up
    if(dist < SYX_EPSILON)
      normalA = Vec3::UnitY;
    else {
      dist = sqrt(dist);
      normalA = -aToB/dist;
    }

    float penetration = combinedRadius - dist;
    Vec3 worldA = posA - normalA*radiusA;
    Vec3 worldB = posB + normalA*radiusB;
    mNarrowphase->SubmitContact(worldA, worldB, normalA, penetration);
  }
}