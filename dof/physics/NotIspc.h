#pragma once

//Hack copy of the ispc code that can be used for debugging

#include <glm/glm.hpp>
#include "out_ispc/unity.h"

#define UNIFORM
#define FLOAT2 glm::vec2
#define EXPORT
#define FOREACH(value, initial, count) for(size_t value = initial; value < count; ++value)
#define UINT32 uint32_t
#define PRAGMA_IGNORE_PERF
#define INLINE inline

namespace notispc {

INLINE float vec2Length(FLOAT2 v) {
  return sqrt(v.x*v.x + v.y*v.y);
}

INLINE FLOAT2 vec2Negate(FLOAT2 v) {
  FLOAT2 result = { -v.x, -v.y };
  return result;
}

INLINE FLOAT2 vec2Sub(FLOAT2 l, FLOAT2 r) {
  FLOAT2 result = { l.x - r.x, l.y - r.y };
  return result;
}

INLINE FLOAT2 vec2Add(FLOAT2 l, FLOAT2 r) {
  FLOAT2 result = { l.x + r.x, l.y + r.y };
  return result;
}

INLINE FLOAT2 vec2Multiply(FLOAT2 v, float scalar) {
  FLOAT2 result = { v.x*scalar, v.y*scalar };
  return result;
}

INLINE float safeDivide(float num, float denom) {
  if(abs(denom) > 0.00001f) {
    return num/denom;
  }
  return 0.0f;
}

INLINE float crossProduct(float ax, float ay, float bx, float by) {
  //[ax] x [bx] = [ax*by - ay*bx]
  //[ay]   [by]
  return ax*by - ay*bx;
}

INLINE FLOAT2 orthogonal(float x, float y) {
  //Cross product with unit Z since everything in 2D is orthogonal to Z
  //[x] [0] [ y]
  //[y]x[0]=[-x]
  //[0] [1] [ 0]
  FLOAT2 result = { y, -x };
  return result;
}

INLINE float dotProduct(float ax, float ay, float bx, float by) {
  return ax*bx + ay*by;
}

INLINE float dotProduct2(FLOAT2 l, FLOAT2 r) {
  return l.x*r.x + l.y*r.y;
}

INLINE FLOAT2 transposeRotation(float cosAngle, float sinAngle) {
  //Rotation matrix is
  //[cos(x), -sin(x)]
  //[sin(x), cos(x)]
  //So the transpose given cos and sin is negating sin
  FLOAT2 result = { cosAngle, -sinAngle };
  return result;
}

//Multiply rotation matrices A*B represented by cos and sin since they're symmetric
INLINE FLOAT2 multiplyRotationMatrices(float cosAngleA, float sinAngleA, float cosAngleB, float sinAngleB) {
  //[cosAngleA, -sinAngleA]*[cosAngleB, -sinAngleB] = [cosAngleA*cosAngleB - sinAngleA*sinAngleB, ...]
  //[sinAngleA, cosAngleA]  [sinAngleB, cosAngleB]    [sinAngleA*cosAngleB + cosAngleA*sinAngleB, ...]
  FLOAT2 result = { cosAngleA*cosAngleB - sinAngleA*sinAngleB, sinAngleA*cosAngleB + cosAngleA*sinAngleB };
  return result;
}

//Multiply M*V where M is the rotation matrix represented by cos/sinangle and V is a vector
INLINE FLOAT2 multiplyVec2ByRotation(float cosAngle, float sinAngle, float vx, float vy) {
  //[cosAngle, -sinAngle]*[vx] = [cosAngle*vx - sinAngle*vy]
  //[sinAngle,  cosAngle] [vy]   [sinAngle*vx + cosAngle*vy]
  FLOAT2 result = { cosAngle*vx - sinAngle*vy, sinAngle*vx + cosAngle*vy };
  return result;
}

//Get the relative right represented by this rotation matrix, in other words the first basis vector (first column of matrix)
INLINE FLOAT2 getRightFromRotation(float cosAngle, float sinAngle) {
  //It already is the first column
  FLOAT2 result = { cosAngle, sinAngle };
  return result;
}

//Get the second basis vector (column)
INLINE FLOAT2 getUpFromRotation(float cosAngle, float sinAngle) {
  //[cosAngle, -sinAngle]
  //[sinAngle,  cosAngle]
  FLOAT2 result = { -sinAngle, cosAngle };
  return result;
}


INLINE   float clamp(float v, float min, float max) {
    return glm::clamp(v, min, max);
  }

INLINE   void solveContactConstraints(
    ispc::UniformContactConstraintPairData& constraints,
    ispc::UniformConstraintObject& objectA,
    ispc::UniformConstraintObject& objectB,
    float lambdaSumOne[],
    float lambdaSumTwo[],
    float frictionLambdaSumOne[],
    float frictionLambdaSumTwo[],
    uint8_t isEnabled[],
    float frictionCoeff,
    uint32_t objectOffset,
    uint32_t start,
    uint32_t count
  ) {
    for(int t = 0; t < (int)count; ++t) {
      const int i = t + start;
      const int oi = i + objectOffset;
      if(!isEnabled[oi]) {
        continue;
      }
      const float nx = constraints.linearAxisX[i];
      const float ny = constraints.linearAxisY[i];

      //Solve friction first. This is a bit silly for the first iteration because the lambda sum is based on the contact sums
      //However, later solved constraints are more likely to be satisfied and contacs are more important than friction
      const glm::vec2 frictionNormal = orthogonal(nx, ny);
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

      const glm::vec2 frictionLinearImpulse = orthogonal(constraints.linearImpulseX[i], constraints.linearImpulseY[i]);
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
      newLambdaSum = std::max(0.0f, lambdaOne + originalLambdaSum);
      lambdaOne = newLambdaSum - originalLambdaSum;
      //Store for next iteration
      //lambdaSumOne[i] = newLambdaSum;

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
      newLambdaSum = std::max(0.0f, lambdaTwo + originalLambdaSum);
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
        case ispc::NoSync: break;
        case ispc::SyncToIndexA: {
          objectA.linVelX[syncA] = objectA.linVelX[oi];
          objectA.linVelY[syncA] = objectA.linVelY[oi];
          objectA.angVel[syncA] = objectA.angVel[oi];
          break;
        }
        case ispc::SyncToIndexB: {
          objectB.linVelX[syncA] = objectA.linVelX[oi];
          objectB.linVelY[syncA] = objectA.linVelY[oi];
          objectB.angVel[syncA] = objectA.angVel[oi];
          break;
        }
      }

      const int syncB = objectB.syncIndex[oi];
      switch (objectB.syncType[oi]) {
        case ispc::NoSync: break;
        case ispc::SyncToIndexA: {
          objectA.linVelX[syncB] = objectB.linVelX[oi];
          objectA.linVelY[syncB] = objectB.linVelY[oi];
          objectA.angVel[syncB] = objectB.angVel[oi];
          break;
        }
        case ispc::SyncToIndexB: {
          objectB.linVelX[syncB] = objectB.linVelX[oi];
          objectB.linVelY[syncB] = objectB.linVelY[oi];
          objectB.angVel[syncB] = objectB.angVel[oi];
          break;
        }
      }
    }
  }

INLINE   void solveContactConstraintsBZeroMass(
    ispc::UniformContactConstraintPairData& constraints,
    ispc::UniformConstraintObject& objectA,
    ispc::UniformConstraintObject& objectB,
    float lambdaSumOne[],
    float lambdaSumTwo[],
    float frictionLambdaSumOne[],
    float frictionLambdaSumTwo[],
    float frictionCoeff,
    uint32_t objectOffset,
    uint32_t start,
    uint32_t count
  ) {
    for(int t = 0; t < (int)count; ++t) {
      const int i = start + t;
      const int oi = i + objectOffset;
      const float nx = constraints.linearAxisX[i];
      const float ny = constraints.linearAxisY[i];

      //Solve friction first. This is a bit silly for the first iteration because the lambda sum is based on the contact sums
      //However, later solved constraints are more likely to be satisfied and contacs are more important than friction
      const glm::vec2 frictionNormal = orthogonal(nx, ny);
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

      const glm::vec2 frictionLinearImpulse = orthogonal(constraints.linearImpulseX[i], constraints.linearImpulseY[i]);
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
      newLambdaSum = std::max(0.0f, lambdaOne + originalLambdaSum);
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
      newLambdaSum = std::max(0.0f, lambdaTwo + originalLambdaSum);
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
      case ispc::NoSync: break;
      case ispc::SyncToIndexA: {
          objectA.linVelX[syncA] = objectA.linVelX[oi];
          objectA.linVelY[syncA] = objectA.linVelY[oi];
          objectA.angVel[syncA] = objectA.angVel[oi];
          break;
        }
      //This would only happen at the end of the static objects table to sync this A as a B for a non-static constraint pair
      case ispc::SyncToIndexB: {
        objectB.linVelX[syncA] = objectA.linVelX[oi];
        objectB.linVelY[syncA] = objectA.linVelY[oi];
        objectB.angVel[syncA] = objectA.angVel[oi];
        break;
      }

      }
    }
  }

using namespace ispc;

template<class A, class B>
INLINE auto min(A a, B b) {
  return glm::min(a, b);
}

template<class A, class B>
INLINE auto max(A a, B b) {
  return glm::max(a, b);
}


//Since these are unit squares the intersect time is also the distance along the line
//since any edge would be of length one
//line is the location of the line on the perpendicular axis, so if intersecting with the X axis it's the Y position of the line along the X axis
//Given start of segment 's' and end 'e' with edge position E, this computes the intersection time t such that the intersection point I is:
//I = (s + (e-s)*t)
//Using the X axis as an example, we already know the y coordinate of I is E, since that's where the edge is
//Iy = (sy + (ey - sy)*t
//Iy = E
//Substitute E for Iy and rearrange to solve for T
//t = (E - sy)/(ey - sy)
//The other probably easier way to think about that is it's the distance from the start to the intersection divided by the total distance
INLINE FLOAT2 getSegmentLineIntersectTimeAndOverlapAlongAxis1D(float line, float segmentBegin, float segmentEnd) {
  const float length = segmentEnd - segmentBegin;
  float t = -1.0f;
  //If this is zero the line is parallel to axis, no intersection
  if(abs(length) > 0.00001f) {
    t = (line - segmentBegin)/length;
  }
  //distance from end of the line to the intersection point on the line
  float overlap = abs(length)*(1.0f - t);
  FLOAT2 result = { t, overlap };
  return result;
}

INLINE int addIndexWrapped(int index, int toAdd, int size) {
  int result = index + toAdd;
  if(result >= size) {
    return 0;
  }
  if(result < 0) {
    //Fine because this is always called with non-zero size
    return size - 1;
  }
  return result;
}

//TODO: this ended up pretty messy. It could be optimized in small ways by changing back to single-axis based computations
//It is likely possible to take better advantage of ispc by splitting each plane clipping phase into its own pass,
//like clipping all points across all collision pairs along the x axes
INLINE EXPORT void generateUnitCubeCubeContacts(
  UNIFORM UniformConstVec2& positionsA,
  UNIFORM UniformRotation& rotationsA,
  UNIFORM UniformConstVec2& positionsB,
  UNIFORM UniformRotation& rotationsB,
  UNIFORM UniformVec2& resultNormals,
  UNIFORM UniformContact& resultContactOne,
  UNIFORM UniformContact& resultContactTwo,
  UNIFORM UINT32 count
) {
  UNIFORM const FLOAT2 aNormals[4] = { { 0.0f, 1.0f }, { 0.0f, -1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f } };

  FOREACH(i, 0, count) {
    //Transpose to undo the rotation of A
    const FLOAT2 rotA = { rotationsA.cosAngle[i], rotationsA.sinAngle[i] };
    const FLOAT2 rotAInverse = transposeRotation(rotA.x, rotA.y);
    const FLOAT2 posA = { positionsA.x[i], positionsA.y[i] };

    const FLOAT2 rotB = { rotationsB.cosAngle[i], rotationsB.sinAngle[i] };
    const FLOAT2 posB = { positionsB.x[i], positionsB.y[i] };
    //B's rotation in A's local space. Transforming to local space A allows this to be solved as computing
    //contacts between an AABB and an OBB instead of OBB to OBB
    const FLOAT2 rotBInA = multiplyRotationMatrices(rotB.x, rotB.y, rotAInverse.x, rotAInverse.y);
    //Get basis vectors with the lengths of B so that they go from the center to the extents
    const FLOAT2 upB = vec2Multiply(getUpFromRotation(rotBInA.x, rotBInA.y), 0.5f);
    const FLOAT2 rightB = vec2Multiply(getRightFromRotation(rotBInA.x, rotBInA.y), 0.5f);
    FLOAT2 posBInA = vec2Sub(posB, posA);
    posBInA = multiplyVec2ByRotation(rotAInverse.x, rotAInverse.y, posBInA.x, posBInA.y);

    //Sutherland hodgman clipping of B in the space of A, meaning all the clipping planes are cardinal axes
    int outputCount = 0;
    //8 should be the maximum amount of points that can result from clipping a square against another, which is when they are inside each-other and all corners of one intersect the edges of the other
    FLOAT2 outputPoints[8];

    //Upper right, lower right, lower left, upper left
    outputPoints[0] = vec2Add(vec2Add(posBInA, upB), rightB);
    outputPoints[1] = vec2Add(vec2Sub(posBInA, upB), rightB);
    outputPoints[2] = vec2Sub(vec2Sub(posBInA, upB), rightB);
    outputPoints[3] = vec2Sub(vec2Add(posBInA, upB), rightB);
    outputCount = 4;

    //ispc prefers single floats even here to avoid "gather required to load value" performance warnings
    float inputPointsX[8];
    float inputPointsY[8];
    int inputCount = 0;
    float bestOverlap = 999.9f;
    FLOAT2 bestNormal = aNormals[0];

    bool allPointsInside = true;

    for(UNIFORM int edgeA = 0; edgeA < 4; ++edgeA) {
      //Copy previous output to current input
      inputCount = outputCount;
      FLOAT2 lastPoint;
      for(int j = 0; j < inputCount; ++j) {
        lastPoint.x = inputPointsX[j] = outputPoints[j].x;
        lastPoint.y = inputPointsY[j] = outputPoints[j].y;
      }
      //This will happen as soon as a separating axis is found as all points will land outside the clip edge and get discarded
      if(!outputCount) {
        break;
      }
      outputCount = 0;

      //ispc doesn't like reading varying from array by index
      UNIFORM FLOAT2 aNormal = aNormals[edgeA];

      //Last inside is invalidated when the edges change since it's inside relative to a given edge
      float lastOverlap = 0.5f - dotProduct(aNormal.x, aNormal.y, lastPoint.x, lastPoint.y);
      bool lastInside = lastOverlap >= 0.0f;

      float currentEdgeOverlap = 0.0f;
      for(int j = 0; j < inputCount; ++j) {
        const FLOAT2 currentPoint = { inputPointsX[j], inputPointsY[j] };
        //(e-p).n
        const float currentOverlap = 0.5f - dotProduct(aNormal.x, aNormal.y, currentPoint.x, currentPoint.y);
        const bool currentInside = currentOverlap >= 0;
        const FLOAT2 lastToCurrent = vec2Sub(currentPoint, lastPoint);
        //Might be division by zero but if so the intersect won't be used because currentInside would match lastInside
        //(e-p).n/(e-s).n
        const float t = 1.0f - (abs(currentOverlap)/abs(dotProduct(aNormal.x, aNormal.y, lastToCurrent.x, lastToCurrent.y)));
        const FLOAT2 intersect = vec2Add(lastPoint, vec2Multiply(lastToCurrent, t));

        currentEdgeOverlap = max(currentOverlap, currentEdgeOverlap);

        //TODO: re-use subtraction above in intersect calculation below
        if(currentInside) {
          allPointsInside = false;
          if(!lastInside) {
            //Went from outside to inside, add intersect
            PRAGMA_IGNORE_PERF
            outputPoints[outputCount] = intersect;
            ++outputCount;
          }
          //Is inside, add current
          PRAGMA_IGNORE_PERF
          outputPoints[outputCount] = currentPoint;
          ++outputCount;
        }
        else if(lastInside) {
          //Went from inside to outside, add intersect.
          PRAGMA_IGNORE_PERF
          outputPoints[outputCount] = intersect;
          ++outputCount;
        }

        lastPoint = currentPoint;
        lastInside = currentInside;
      }

      //Keep track of the least positive overlap for the final results
      if(currentEdgeOverlap < bestOverlap) {
        bestOverlap = currentEdgeOverlap;
        bestNormal = aNormal;
      }
    }

    if(outputCount == 0) {
      //No collision, store negative overlap to indicate this
      resultContactOne.overlap[i] = -1.0f;
      resultContactTwo.overlap[i] = -1.0f;
    }
    else if(allPointsInside) {
      //Niche case where one shape is entirely inside another. Overlap is only determined
      //for intersect points which is fine for all cases except this one
      //Return arbitrary contacts here. Not too worried about accuracy because collision resolution has broken down if this happened
      resultContactOne.overlap[i] = 0.5f;
      resultContactOne.x[i] = posB.x;
      resultContactOne.y[i] = posB.y;
      resultContactTwo.overlap[i] = -1.0f;
      resultNormals.x[i] = 1.0f;
      resultNormals.x[i] = 0.0f;
    }
    else {
      //TODO: need to figure out a better way to do this
      //Also need to try the axes of B to see if they produce a better normal than what was found through clipping
      FLOAT2 candidateNormals[3] = { bestNormal, getUpFromRotation(rotBInA.x, rotBInA.y), getRightFromRotation(rotBInA.x, rotBInA.y) };
      const FLOAT2 originalBestNormal = bestNormal;
      float bestNormalDiff = 99.0f;
      //Figuring out the best normal has two parts:
      // - Determining which results  in the least overlap along the normal, which is the distance between the two extremes of projections of all clipped points onto normal
      // - Determine the sign of the normal
      // This loop will do the former by determining all the projections on the normal and picking the normal that has the greatest difference
      // Then the result can be flipped so it's pointing in the same direction as the original best
      // This is a hacky assumption based on that either the original best will be chosen or one not far off from it
      for(UNIFORM int j = 0; j < 3; ++j) {
        float thisMin = 999.0f;
        float thisMax = -thisMin;
        FLOAT2 normal = candidateNormals[j];
        for(UNIFORM int k = 0; k < outputCount; ++k) {
          float thisOverlap = dotProduct2(normal, outputPoints[k]);
          thisMin = min(thisMin, thisOverlap);
          thisMax = max(thisMax, thisOverlap);
        }
        //This is the absolute value of the total amount of overlap along this axis, we're looking for the normal with the smallest overlap
        const float thisNormalDiff = thisMax - thisMin;
        if(thisNormalDiff < bestNormalDiff) {
          bestNormalDiff = thisNormalDiff;
          bestNormal = normal;
        }
      }

      //Now the best normal is known, make sure it's pointing in a similar direction to the original
      if(dotProduct2(originalBestNormal, bestNormal) < 0.0f) {
        bestNormal = vec2Negate(bestNormal);
      }

      //Now find the two best contact points. The normal is going away from A, so the smallest projection is the one with the most overlap, since it's going most against the normal
      //The overlap for any point is the distance of its projection from the greatest projection: the point furthest away from A
      FLOAT2 bestPoint = outputPoints[0];
      FLOAT2 secondBestPoint;
      float minProjection = 999.0f;
      float secondMinProjection = minProjection;
      float maxProjection = -1;
      for(UNIFORM int j = 0; j < outputCount; ++j) {
        const FLOAT2 thisPoint = outputPoints[j];
        const float thisProjection = dotProduct2(bestNormal, thisPoint);
        maxProjection = max(maxProjection, thisProjection);
        if(thisProjection < minProjection) {
          secondMinProjection = minProjection;
          secondBestPoint = bestPoint;

          minProjection = thisProjection;
          bestPoint = thisPoint;
        }
        else if(thisProjection < secondMinProjection) {
          secondMinProjection = thisProjection;
          secondBestPoint = thisPoint;
        }
      }

      //Contacts are the two most overlapping points along the normal axis
      FLOAT2 contactOne = bestPoint;
      FLOAT2 contactTwo = secondBestPoint;
      float contactTwoOverlap = maxProjection - secondMinProjection;
      float contactOneOverlap = maxProjection - minProjection;

      //Transform the contacts back to world
      contactOne = vec2Add(posA, multiplyVec2ByRotation(rotA.x, rotA.y, contactOne.x, contactOne.y));
      contactTwo = vec2Add(posA, multiplyVec2ByRotation(rotA.x, rotA.y, contactTwo.x, contactTwo.y));

      //Transform normal to world
      bestNormal = multiplyVec2ByRotation(rotA.x, rotA.y, bestNormal.x, bestNormal.y);
      //Flip from being a face on A to going towards A
      resultNormals.x[i] = -bestNormal.x;
      resultNormals.y[i] = -bestNormal.y;

      //Store the final results
      resultContactOne.x[i] = contactOne.x;
      resultContactOne.y[i] = contactOne.y;
      resultContactOne.overlap[i] = contactOneOverlap;

      resultContactTwo.x[i] = contactTwo.x;
      resultContactTwo.y[i] = contactTwo.y;
      resultContactTwo.overlap[i] = contactTwoOverlap;
    }
  }
}
}