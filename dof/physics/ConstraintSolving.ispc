#include "IspcMath.h"
#include "Uniforms.h"

export void setupConstraintsSharedMass(
  uniform float mass,
  uniform float inertia,
  uniform float biasMultiplier,
  uniform UniformVec2& contactNormal,
  uniform UniformVec2& aToContact,
  uniform UniformVec2& bToContact,
  uniform float contactOverlap[],
  uniform UniformConstraintData& constraints,
  uniform uint32 count
) {
  //This is length(linearAxisA*massA) + length(linearAxisB*massB)
  //Since the axes are normalized it simplifies to massA + massB, and they both have the same mass
  const float linearConstraintMass = mass + mass;
  foreach(i = 0 ... count) {
    const float nx = contactNormal.x[i];
    const float ny = contactNormal.y[i];
    constraints.linearAxisX[i] = nx;
    constraints.linearAxisY[i] = ny;

    constraints.angularAxisA[i] = crossProduct(aToContact.x[i], aToContact.y[i], nx, ny);
    constraints.angularAxisB[i] = crossProduct(bToContact.x[i], bToContact.y[i], -nx, -ny);

    constraints.linearImpulseX[i] = nx*mass;
    constraints.linearImpulseY[i] = ny*mass;

    constraints.angularImpulseA[i] = constraints.angularAxisA[i]*inertia;
    constraints.angularImpulseB[i] = constraints.angularAxisB[i]*inertia;

    //Normally dot product of angular axis with itself, resulting in the square magnitude. In 2D it's always a rotation around the z axis, so it's the z component of rotation times itself
    const float angularConstraintMass = constraints.angularImpulseA[i]*constraints.angularImpulseA[i] + constraints.angularImpulseB[i]*constraints.angularImpulseB[i];
    constraints.constraintMass[i] = safeDivide(1.0f, linearConstraintMass + angularConstraintMass);

    //Still solve constraint if the objects are only slightly separated
    if(contactOverlap[i] >= -0.01f) {
      //If overlap is positive, there is collision, use the bias to resolve the overlap
      constraints.bias[i] = -max(0.0f, contactOverlap[i]*biasMultiplier);
    }
    else {
      //If the overlap is negative the objects are not colliding.
      //The constraint can be effectively ignored by setting the bias to a large positive amount
      //This will mean that the lambda sum would only become positive if the two objects were moving towards
      //each other larger than that amount, which is impossible if large enough
      //Similarly, this could also be used to try to prevent objects from colliding next frame by avoiding
      //a velocity large enough to close the gap between them.
      //The problem is it could equally well lead to false collisions if it turns out the two wouldn't have collided
      //For now, ignore solving for this case
      constraints.bias[i] = 999999.0f;
    }
  }
}

export void solveContactConstraints(
  uniform UniformConstraintData& constraints,
  uniform UniformConstraintObject& objectA,
  uniform UniformConstraintObject& objectB,
  uniform float lambdaSum[],
  uniform uint32 count
) {
  foreach(i = 0 ... count) {
    const float nx = constraints.linearAxisX[i];
    const float ny = constraints.linearAxisY[i];
    const float jv = dotProduct(objectA.linVelX[i], objectA.linVelY[i], nx, ny) - dotProduct(objectB.linVelX[i], objectB.linVelY[i], nx, ny)
      + objectA.angVel[i]*constraints.angularAxisA[i] + objectB.angVel[i]*constraints.angularAxisB[i];

    //Compute the impulse multiplier
    float lambda = -(jv + constraints.bias[i])*constraints.constraintMass[i];
    const float originalLambdaSum = lambdaSum[i];
    //Clamp lambda bounds, which for a contact constraint means > 0
    const float newLambdaSum = max(0.0f, lambda + originalLambdaSum);
    lambda = newLambdaSum - originalLambdaSum;
    //Store for next iteration
    lambdaSum[i] = newLambdaSum;

    //Apply the impulse along the constraint axis using the computed multiplier
    objectA.linVelX[i] += lambda*constraints.linearImpulseX[i];
    objectA.linVelY[i] += lambda*constraints.linearImpulseY[i];
    objectB.linVelX[i] -= lambda*constraints.linearImpulseX[i];
    objectB.linVelY[i] -= lambda*constraints.linearImpulseY[i];
    objectA.angVel[i] += lambda*constraints.angularImpulseA[i];
    objectB.angVel[i] += lambda*constraints.angularImpulseB[i];

    //This is the inefficient unavoidable part. Hopefully the caller can sort the pairs so that this happens as little as possible
    //This allows duplicate pairs to exist by copying the data forward to the next duplicate occurrence. This duplication is ordered
    //carefully to avoid the need to copy within a simd lane
    const int syncA = objectA.syncIndex[i];
    switch (objectA.syncType[i]) {
      case NoSync: break;
      case SyncToIndexA: {
        #pragma ignore warning(perf)
        objectA.linVelX[syncA] = objectA.linVelX[i];
        #pragma ignore warning(perf)
        objectA.linVelY[syncA] = objectA.linVelY[i];
        #pragma ignore warning(perf)
        objectA.angVel[syncA] = objectA.angVel[i];
        break;
      }
      case SyncToIndexB: {
        #pragma ignore warning(perf)
        objectB.linVelX[syncA] = objectA.linVelX[i];
        #pragma ignore warning(perf)
        objectB.linVelY[syncA] = objectA.linVelY[i];
        #pragma ignore warning(perf)
        objectB.angVel[syncA] = objectA.angVel[i];
        break;
      }
    }

    const int syncB = objectB.syncIndex[i];
    switch (objectB.syncType[i]) {
      case NoSync: break;
      case SyncToIndexA: {
        #pragma ignore warning(perf)
        objectA.linVelX[syncB] = objectB.linVelX[i];
        #pragma ignore warning(perf)
        objectA.linVelY[syncB] = objectB.linVelY[i];
        #pragma ignore warning(perf)
        objectA.angVel[syncB] = objectB.angVel[i];
        break;
      }
      case SyncToIndexB: {
        #pragma ignore warning(perf)
        objectB.linVelX[syncB] = objectB.linVelX[i];
        #pragma ignore warning(perf)
        objectB.linVelY[syncB] = objectB.linVelY[i];
        #pragma ignore warning(perf)
        objectB.angVel[syncB] = objectB.angVel[i];
        break;
      }
    }
  }
}