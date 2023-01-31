#include "IspcMath.h"
#include "Uniforms.h"

export void setupConstraintsSharedMass(
  uniform float mass,
  uniform float inertia,
  uniform float biasMultiplier,
  uniform UniformVec2& contactNormal,
  uniform UniformVec2& aToContactOne,
  uniform UniformVec2& aToContactTwo,
  uniform UniformVec2& bToContactOne,
  uniform UniformVec2& bToContactTwo,
  uniform float contactOverlapOne[],
  uniform float contactOverlapTwo[],
  uniform UniformContactConstraintPairData& constraints,
  uniform uint32 count
) {
  //This is length(linearAxisA*massA) + length(linearAxisB*massB)
  //Since the axes are normalized it simplifies to massA + massB, and they both have the same mass
  const float linearConstraintMass = mass + mass;
  foreach(i = 0 ... count) {
    const float nx = contactNormal.x[i];
    const float ny = contactNormal.y[i];
    //Linear axis is the same for both contacts
    constraints.linearAxisX[i] = nx;
    constraints.linearAxisY[i] = ny;

    constraints.angularAxisOneA[i] = crossProduct(aToContactOne.x[i], aToContactOne.y[i], nx, ny);
    constraints.angularAxisOneB[i] = crossProduct(bToContactOne.x[i], bToContactOne.y[i], -nx, -ny);
    constraints.angularAxisTwoA[i] = crossProduct(aToContactTwo.x[i], aToContactTwo.y[i], nx, ny);
    constraints.angularAxisTwoB[i] = crossProduct(bToContactTwo.x[i], bToContactTwo.y[i], -nx, -ny);

    //This orthogonal trick is used for all the linear terms to avoid needing to store more vectors
    const float<2> frictionNormal = orthogonal(nx, ny);
    constraints.angularFrictionAxisOneA[i] = crossProduct(aToContactOne.x[i], aToContactOne.y[i], frictionNormal.x, frictionNormal.y);
    constraints.angularFrictionAxisOneB[i] = crossProduct(bToContactOne.x[i], bToContactOne.y[i], -frictionNormal.x, -frictionNormal.y);
    constraints.angularFrictionAxisTwoA[i] = crossProduct(aToContactTwo.x[i], aToContactTwo.y[i], frictionNormal.x, frictionNormal.y);
    constraints.angularFrictionAxisTwoB[i] = crossProduct(bToContactTwo.x[i], bToContactTwo.y[i], -frictionNormal.x, -frictionNormal.y);

    //Same for both contacts, and friction can use it by flipping it to orthogonal
    constraints.linearImpulseX[i] = nx*mass;
    constraints.linearImpulseY[i] = ny*mass;

    //Angular impulse is different for both contacts since the r-vector differs
    constraints.angularImpulseOneA[i] = constraints.angularAxisOneA[i]*inertia;
    constraints.angularImpulseOneB[i] = constraints.angularAxisOneB[i]*inertia;
    constraints.angularImpulseTwoA[i] = constraints.angularAxisTwoA[i]*inertia;
    constraints.angularImpulseTwoB[i] = constraints.angularAxisTwoB[i]*inertia;

    constraints.angularFrictionImpulseOneA[i] = constraints.angularFrictionAxisOneA[i]*inertia;
    constraints.angularFrictionImpulseOneB[i] = constraints.angularFrictionAxisOneB[i]*inertia;
    constraints.angularFrictionImpulseTwoA[i] = constraints.angularFrictionAxisTwoA[i]*inertia;
    constraints.angularFrictionImpulseTwoB[i] = constraints.angularFrictionAxisTwoB[i]*inertia;

    //Normally dot product of angular axis with itself, resulting in the square magnitude. In 2D it's always a rotation around the z axis, so it's the z component of rotation times itself
    const float angularConstraintMassOne = constraints.angularImpulseOneA[i]*constraints.angularAxisOneA[i] + constraints.angularImpulseOneB[i]*constraints.angularAxisOneB[i];
    constraints.constraintMassOne[i] = safeDivide(1.0f, linearConstraintMass + angularConstraintMassOne);
    const float angularConstraintMassTwo = constraints.angularImpulseTwoA[i]*constraints.angularAxisTwoA[i] + constraints.angularImpulseTwoB[i]*constraints.angularAxisTwoB[i];
    constraints.constraintMassTwo[i] = safeDivide(1.0f, linearConstraintMass + angularConstraintMassTwo);

    const float angularFrictionConstraintMassOne = constraints.angularFrictionImpulseOneA[i]*constraints.angularFrictionAxisOneA[i] + constraints.angularFrictionImpulseOneB[i]*constraints.angularFrictionAxisOneB[i];
    constraints.frictionConstraintMassOne[i] = safeDivide(1.0f, linearConstraintMass + angularFrictionConstraintMassOne);
    const float angularFrictionConstraintMassTwo = constraints.angularFrictionImpulseTwoA[i]*constraints.angularFrictionAxisTwoA[i] + constraints.angularFrictionImpulseTwoB[i]*constraints.angularFrictionAxisTwoB[i];
    constraints.frictionConstraintMassTwo[i] = safeDivide(1.0f, linearConstraintMass + angularFrictionConstraintMassTwo);
    //No bias for friction constraints

    //Still solve constraint if the objects are only slightly separated
    uniform const float slop = 0.005f;
    if(contactOverlapOne[i] >= -0.01f) {
      //If overlap is positive, there is collision, use the bias to resolve the overlap
      constraints.biasOne[i] = -max(0.0f, contactOverlapOne[i]*biasMultiplier - slop);
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
      constraints.biasOne[i] = 999999.0f;
    }
    if(contactOverlapTwo[i] >= -0.01f) {
      constraints.biasTwo[i] = -max(0.0f, contactOverlapTwo[i]*biasMultiplier - slop);
    }
    else {
      constraints.biasTwo[i] = 999999.0f;
    }
  }
}

export void setupConstraintsSharedMassBZeroMass(
  uniform float mass,
  uniform float inertia,
  uniform float biasMultiplier,
  uniform UniformVec2& contactNormal,
  uniform UniformVec2& aToContactOne,
  uniform UniformVec2& aToContactTwo,
  uniform float contactOverlapOne[],
  uniform float contactOverlapTwo[],
  uniform UniformContactConstraintPairData& constraints,
  uniform uint32 count
) {
  //This is length(linearAxisA*massA)
  //Since the axes are normalized it simplifies to massA + massB, and massB is zero
  const float linearConstraintMass = mass;
  foreach(i = 0 ... count) {
    const float nx = contactNormal.x[i];
    const float ny = contactNormal.y[i];
    //Linear axis is the same for both contacts
    constraints.linearAxisX[i] = nx;
    constraints.linearAxisY[i] = ny;

    constraints.angularAxisOneA[i] = crossProduct(aToContactOne.x[i], aToContactOne.y[i], nx, ny);
    constraints.angularAxisTwoA[i] = crossProduct(aToContactTwo.x[i], aToContactTwo.y[i], nx, ny);

    //This orthogonal trick is used for all the linear terms to avoid needing to store more vectors
    const float<2> frictionNormal = orthogonal(nx, ny);
    constraints.angularFrictionAxisOneA[i] = crossProduct(aToContactOne.x[i], aToContactOne.y[i], frictionNormal.x, frictionNormal.y);
    constraints.angularFrictionAxisTwoA[i] = crossProduct(aToContactTwo.x[i], aToContactTwo.y[i], frictionNormal.x, frictionNormal.y);

    //Same for both contacts, and friction can use it by flipping it to orthogonal
    constraints.linearImpulseX[i] = nx*mass;
    constraints.linearImpulseY[i] = ny*mass;

    //Angular impulse is different for both contacts since the r-vector differs
    constraints.angularImpulseOneA[i] = constraints.angularAxisOneA[i]*inertia;
    constraints.angularImpulseTwoA[i] = constraints.angularAxisTwoA[i]*inertia;

    constraints.angularFrictionImpulseOneA[i] = constraints.angularFrictionAxisOneA[i]*inertia;
    constraints.angularFrictionImpulseTwoA[i] = constraints.angularFrictionAxisTwoA[i]*inertia;

    //Normally dot product of angular axis with itself, resulting in the square magnitude. In 2D it's always a rotation around the z axis, so it's the z component of rotation times itself
    const float angularConstraintMassOne = constraints.angularImpulseOneA[i]*constraints.angularAxisOneA[i];
    constraints.constraintMassOne[i] = safeDivide(1.0f, linearConstraintMass + angularConstraintMassOne);
    const float angularConstraintMassTwo = constraints.angularImpulseTwoA[i]*constraints.angularAxisTwoA[i];
    constraints.constraintMassTwo[i] = safeDivide(1.0f, linearConstraintMass + angularConstraintMassTwo);

    const float angularFrictionConstraintMassOne = constraints.angularFrictionImpulseOneA[i]*constraints.angularFrictionAxisOneA[i];
    constraints.frictionConstraintMassOne[i] = safeDivide(1.0f, linearConstraintMass + angularFrictionConstraintMassOne);
    const float angularFrictionConstraintMassTwo = constraints.angularFrictionImpulseTwoA[i]*constraints.angularFrictionAxisTwoA[i];
    constraints.frictionConstraintMassTwo[i] = safeDivide(1.0f, linearConstraintMass + angularFrictionConstraintMassTwo);
    //No bias for friction constraints

    uniform const float slop = 0.005f;
    if(contactOverlapOne[i] >= -0.01f) {
      constraints.biasOne[i] = -max(0.0f, contactOverlapOne[i]*biasMultiplier - slop);
    }
    else {
      constraints.biasOne[i] = 999999.0f;
    }
    if(contactOverlapTwo[i] >= -0.01f) {
      constraints.biasTwo[i] = -max(0.0f, contactOverlapTwo[i]*biasMultiplier - slop);
    }
    else {
      constraints.biasTwo[i] = 999999.0f;
    }
  }
}

export void solveContactConstraints(
  uniform UniformContactConstraintPairData& constraints,
  uniform UniformConstraintObject& objectA,
  uniform UniformConstraintObject& objectB,
  uniform float lambdaSumOne[],
  uniform float lambdaSumTwo[],
  uniform float frictionLambdaSumOne[],
  uniform float frictionLambdaSumTwo[],
  uniform uint8 isEnabled[],
  uniform float frictionCoeff,
  uniform uint32 objectOffset,
  uniform uint32 start,
  uniform uint32 count
) {
  foreach(t = 0 ... count) {
    const int i = t + start;
    const int oi = i + objectOffset;
    if(!isEnabled[oi]) {
      continue;
    }
    const float nx = constraints.linearAxisX[i];
    const float ny = constraints.linearAxisY[i];

    //Solve friction first. This is a bit silly for the first iteration because the lambda sum is based on the contact sums
    //However, later solved constraints are more likely to be satisfied and contacs are more important than friction
    const float<2> frictionNormal = orthogonal(nx, ny);
    const float jvFrictionOne = dotProduct(objectA.linVelX[oi], objectA.linVelY[oi], frictionNormal.x, frictionNormal.y) - dotProduct(objectB.linVelX[oi], objectB.linVelY[oi], frictionNormal.x, frictionNormal.y)
      + objectA.angVel[oi]*constraints.angularFrictionAxisOneA[i] + objectB.angVel[oi]*constraints.angularFrictionAxisOneB[i];

    //Friction has no bias
    float frictionLambdaOne = -jvFrictionOne*constraints.frictionConstraintMassOne[i];

    //Limit of friction constraint is the normal force from the contact constraint, so the contact's lambda
    float originalLambdaSum = frictionLambdaSumOne[i];
    //Since contact sums are always positive the negative here is known to actually be negative
    const float frictionLimitOne = lambdaSumOne[i]*frictionCoeff;
    float newLambdaSum = clamp(frictionLambdaOne + originalLambdaSum, -frictionLimitOne, frictionLimitOne);
    frictionLambdaOne = newLambdaSum - originalLambdaSum;
    frictionLambdaSumOne[i] = newLambdaSum;

    const float<2> frictionLinearImpulse = orthogonal(constraints.linearImpulseX[i], constraints.linearImpulseY[i]);
    objectA.linVelX[oi] += frictionLambdaOne*frictionLinearImpulse.x;
    objectA.linVelY[oi] += frictionLambdaOne*frictionLinearImpulse.y;
    objectB.linVelX[oi] -= frictionLambdaOne*frictionLinearImpulse.x;
    objectB.linVelY[oi] -= frictionLambdaOne*frictionLinearImpulse.y;
    objectA.angVel[oi] += frictionLambdaOne*constraints.angularFrictionImpulseOneA[i];
    objectB.angVel[oi] += frictionLambdaOne*constraints.angularFrictionImpulseOneB[i];

    const float jvFrictionTwo = dotProduct(objectA.linVelX[oi], objectA.linVelY[oi], frictionNormal.x, frictionNormal.y) - dotProduct(objectB.linVelX[oi], objectB.linVelY[oi], frictionNormal.x, frictionNormal.y)
      + objectA.angVel[oi]*constraints.angularFrictionAxisTwoA[i] + objectB.angVel[oi]*constraints.angularFrictionAxisTwoB[i];

    float frictionLambdaTwo = -jvFrictionTwo*constraints.frictionConstraintMassTwo[i];

    originalLambdaSum = frictionLambdaSumTwo[i];
    const float frictionLimitTwo = lambdaSumTwo[i]*frictionCoeff;
    newLambdaSum = clamp(frictionLambdaTwo + originalLambdaSum, -frictionLimitTwo, frictionLimitTwo);
    frictionLambdaTwo = newLambdaSum - originalLambdaSum;
    frictionLambdaSumTwo[i] = newLambdaSum;

    objectA.linVelX[oi] += frictionLambdaTwo*frictionLinearImpulse.x;
    objectA.linVelY[oi] += frictionLambdaTwo*frictionLinearImpulse.y;
    objectB.linVelX[oi] -= frictionLambdaTwo*frictionLinearImpulse.x;
    objectB.linVelY[oi] -= frictionLambdaTwo*frictionLinearImpulse.y;
    objectA.angVel[oi] += frictionLambdaTwo*constraints.angularFrictionImpulseTwoA[i];
    objectB.angVel[oi] += frictionLambdaTwo*constraints.angularFrictionImpulseTwoB[i];

    //Solve contact one. Can't be combined with the above unless they are block solved because the velocities affect each-other
    //It might be possible to do friction and contact at the same time since they're orthogonal, not sure about the rotation in that case though
    const float jvOne = dotProduct(objectA.linVelX[oi], objectA.linVelY[oi], nx, ny) - dotProduct(objectB.linVelX[oi], objectB.linVelY[oi], nx, ny)
      + objectA.angVel[oi]*constraints.angularAxisOneA[i] + objectB.angVel[oi]*constraints.angularAxisOneB[i];

    //Compute the impulse multiplier
    float lambdaOne = -(jvOne + constraints.biasOne[i])*constraints.constraintMassOne[i];

    originalLambdaSum = lambdaSumOne[i];
    //Clamp lambda bounds, which for a contact constraint means > 0
    newLambdaSum = max(0.0f, lambdaOne + originalLambdaSum);
    lambdaOne = newLambdaSum - originalLambdaSum;
    //Store for next iteration
    lambdaSumOne[i] = newLambdaSum;

    //Apply the impulse along the constraint axis using the computed multiplier
    objectA.linVelX[oi] += lambdaOne*constraints.linearImpulseX[i];
    objectA.linVelY[oi] += lambdaOne*constraints.linearImpulseY[i];
    objectB.linVelX[oi] -= lambdaOne*constraints.linearImpulseX[i];
    objectB.linVelY[oi] -= lambdaOne*constraints.linearImpulseY[i];
    objectA.angVel[oi] += lambdaOne*constraints.angularImpulseOneA[i];
    objectB.angVel[oi] += lambdaOne*constraints.angularImpulseOneB[i];

    //Solve contact two.
    const float jvTwo = dotProduct(objectA.linVelX[oi], objectA.linVelY[oi], nx, ny) - dotProduct(objectB.linVelX[oi], objectB.linVelY[oi], nx, ny)
      + objectA.angVel[oi]*constraints.angularAxisTwoA[i] + objectB.angVel[oi]*constraints.angularAxisTwoB[i];

    float lambdaTwo = -(jvTwo + constraints.biasTwo[i])*constraints.constraintMassTwo[i];

    originalLambdaSum = lambdaSumTwo[i];
    newLambdaSum = max(0.0f, lambdaTwo + originalLambdaSum);
    lambdaTwo = newLambdaSum - originalLambdaSum;
    lambdaSumTwo[i] = newLambdaSum;

    objectA.linVelX[oi] += lambdaTwo*constraints.linearImpulseX[i];
    objectA.linVelY[oi] += lambdaTwo*constraints.linearImpulseY[i];
    objectB.linVelX[oi] -= lambdaTwo*constraints.linearImpulseX[i];
    objectB.linVelY[oi] -= lambdaTwo*constraints.linearImpulseY[i];
    objectA.angVel[oi] += lambdaTwo*constraints.angularImpulseTwoA[i];
    objectB.angVel[oi] += lambdaTwo*constraints.angularImpulseTwoB[i];

    //This is the inefficient unavoidable part. Hopefully the caller can sort the pairs so that this happens as little as possible
    //This allows duplicate pairs to exist by copying the data forward to the next duplicate occurrence. This duplication is ordered
    //carefully to avoid the need to copy within a simd lane
    const int syncA = objectA.syncIndex[oi];
    switch (objectA.syncType[oi]) {
      case NoSync: break;
      case SyncToIndexA: {
        #pragma ignore warning(perf)
        objectA.linVelX[syncA] = objectA.linVelX[oi];
        #pragma ignore warning(perf)
        objectA.linVelY[syncA] = objectA.linVelY[oi];
        #pragma ignore warning(perf)
        objectA.angVel[syncA] = objectA.angVel[oi];
        break;
      }
      case SyncToIndexB: {
        #pragma ignore warning(perf)
        objectB.linVelX[syncA] = objectA.linVelX[oi];
        #pragma ignore warning(perf)
        objectB.linVelY[syncA] = objectA.linVelY[oi];
        #pragma ignore warning(perf)
        objectB.angVel[syncA] = objectA.angVel[oi];
        break;
      }
    }

    const int syncB = objectB.syncIndex[oi];
    switch (objectB.syncType[oi]) {
      case NoSync: break;
      case SyncToIndexA: {
        #pragma ignore warning(perf)
        objectA.linVelX[syncB] = objectB.linVelX[oi];
        #pragma ignore warning(perf)
        objectA.linVelY[syncB] = objectB.linVelY[oi];
        #pragma ignore warning(perf)
        objectA.angVel[syncB] = objectB.angVel[oi];
        break;
      }
      case SyncToIndexB: {
        #pragma ignore warning(perf)
        objectB.linVelX[syncB] = objectB.linVelX[oi];
        #pragma ignore warning(perf)
        objectB.linVelY[syncB] = objectB.linVelY[oi];
        #pragma ignore warning(perf)
        objectB.angVel[syncB] = objectB.angVel[oi];
        break;
      }
    }
  }
}

export void solveContactConstraintsBZeroMass(
  uniform UniformContactConstraintPairData& constraints,
  uniform UniformConstraintObject& objectA,
  uniform UniformConstraintObject& objectB,
  uniform float lambdaSumOne[],
  uniform float lambdaSumTwo[],
  uniform float frictionLambdaSumOne[],
  uniform float frictionLambdaSumTwo[],
  uniform float frictionCoeff,
  uniform uint32 objectOffset,
  uniform uint32 start,
  uniform uint32 count
) {
  foreach(t = 0 ... count) {
    const int i = t + start;
    const int oi = i + objectOffset;
    const float nx = constraints.linearAxisX[i];
    const float ny = constraints.linearAxisY[i];

    //Solve friction first. This is a bit silly for the first iteration because the lambda sum is based on the contact sums
    //However, later solved constraints are more likely to be satisfied and contacs are more important than friction
    const float<2> frictionNormal = orthogonal(nx, ny);
    const float jvFrictionOne = dotProduct(objectA.linVelX[oi], objectA.linVelY[oi], frictionNormal.x, frictionNormal.y)
      + objectA.angVel[oi]*constraints.angularFrictionAxisOneA[i];

    //Friction has no bias
    float frictionLambdaOne = -jvFrictionOne*constraints.frictionConstraintMassOne[i];

    //Limit of friction constraint is the normal force from the contact constraint, so the contact's lambda
    float originalLambdaSum = frictionLambdaSumOne[i];
    //Since contact sums are always positive the negative here is known to actually be negative
    const float frictionLimitOne = lambdaSumOne[i]*frictionCoeff;
    float newLambdaSum = clamp(frictionLambdaOne + originalLambdaSum, -frictionLimitOne, frictionLimitOne);
    frictionLambdaOne = newLambdaSum - originalLambdaSum;
    frictionLambdaSumOne[i] = newLambdaSum;

    const float<2> frictionLinearImpulse = orthogonal(constraints.linearImpulseX[i], constraints.linearImpulseY[i]);
    objectA.linVelX[oi] += frictionLambdaOne*frictionLinearImpulse.x;
    objectA.linVelY[oi] += frictionLambdaOne*frictionLinearImpulse.y;
    objectA.angVel[oi] += frictionLambdaOne*constraints.angularFrictionImpulseOneA[i];

    const float jvFrictionTwo = dotProduct(objectA.linVelX[oi], objectA.linVelY[oi], frictionNormal.x, frictionNormal.y)
      + objectA.angVel[oi]*constraints.angularFrictionAxisTwoA[i];

    float frictionLambdaTwo = -jvFrictionTwo*constraints.frictionConstraintMassTwo[i];

    originalLambdaSum = frictionLambdaSumTwo[i];
    const float frictionLimitTwo = lambdaSumTwo[i]*frictionCoeff;
    newLambdaSum = clamp(frictionLambdaTwo + originalLambdaSum, -frictionLimitTwo, frictionLimitTwo);
    frictionLambdaTwo = newLambdaSum - originalLambdaSum;
    frictionLambdaSumTwo[i] = newLambdaSum;

    objectA.linVelX[oi] += frictionLambdaTwo*frictionLinearImpulse.x;
    objectA.linVelY[oi] += frictionLambdaTwo*frictionLinearImpulse.y;
    objectA.angVel[oi] += frictionLambdaTwo*constraints.angularFrictionImpulseTwoA[i];

    //Solve contact one. Can't be combined with the above unless they are block solved because the velocities affect each-other
    //It might be possible to do friction and contact at the same time since they're orthogonal, not sure about the rotation in that case though
    const float jvOne = dotProduct(objectA.linVelX[oi], objectA.linVelY[oi], nx, ny)
      + objectA.angVel[oi]*constraints.angularAxisOneA[i];

    //Compute the impulse multiplier
    float lambdaOne = -(jvOne + constraints.biasOne[i])*constraints.constraintMassOne[i];

    originalLambdaSum = lambdaSumOne[i];
    //Clamp lambda bounds, which for a contact constraint means > 0
    newLambdaSum = max(0.0f, lambdaOne + originalLambdaSum);
    lambdaOne = newLambdaSum - originalLambdaSum;
    //Store for next iteration
    lambdaSumOne[i] = newLambdaSum;

    //Apply the impulse along the constraint axis using the computed multiplier
    objectA.linVelX[oi] += lambdaOne*constraints.linearImpulseX[i];
    objectA.linVelY[oi] += lambdaOne*constraints.linearImpulseY[i];
    objectA.angVel[oi] += lambdaOne*constraints.angularImpulseOneA[i];

    //Solve contact two.
    const float jvTwo = dotProduct(objectA.linVelX[oi], objectA.linVelY[oi], nx, ny)
      + objectA.angVel[oi]*constraints.angularAxisTwoA[i];

    float lambdaTwo = -(jvTwo + constraints.biasTwo[i])*constraints.constraintMassTwo[i];

    originalLambdaSum = lambdaSumTwo[i];
    newLambdaSum = max(0.0f, lambdaTwo + originalLambdaSum);
    lambdaTwo = newLambdaSum - originalLambdaSum;
    lambdaSumTwo[i] = newLambdaSum;

    objectA.linVelX[oi] += lambdaTwo*constraints.linearImpulseX[i];
    objectA.linVelY[oi] += lambdaTwo*constraints.linearImpulseY[i];
    objectA.angVel[oi] += lambdaTwo*constraints.angularImpulseTwoA[i];

    //This is the inefficient unavoidable part. Hopefully the caller can sort the pairs so that this happens as little as possible
    //This allows duplicate pairs to exist by copying the data forward to the next duplicate occurrence. This duplication is ordered
    //carefully to avoid the need to copy within a simd lane
    const int syncA = objectA.syncIndex[oi];
    switch (objectA.syncType[oi]) {
      case NoSync: break;
      case SyncToIndexA: {
        #pragma ignore warning(perf)
        objectA.linVelX[syncA] = objectA.linVelX[oi];
        #pragma ignore warning(perf)
        objectA.linVelY[syncA] = objectA.linVelY[oi];
        #pragma ignore warning(perf)
        objectA.angVel[syncA] = objectA.angVel[oi];
        break;
      }
      //This would only happen at the end of the static objects table to sync this A as a B for a non-static constraint pair
      case SyncToIndexB: {
        #pragma ignore warning(perf)
        objectB.linVelX[syncA] = objectA.linVelX[oi];
        #pragma ignore warning(perf)
        objectB.linVelY[syncA] = objectA.linVelY[oi];
        #pragma ignore warning(perf)
        objectB.angVel[syncA] = objectA.angVel[oi];
        break;
      }
    }
  }
}