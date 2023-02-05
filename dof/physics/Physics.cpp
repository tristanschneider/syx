#include "Precompile.h"
#include "Physics.h"

#include "glm/common.hpp"
#include "out_ispc/unity.h"
#include "PhysicsTableIds.h"
#include "TableOperations.h"

#include "glm/detail/func_geometric.inl"

namespace {
  template<class RowT, class TableT>
  decltype(std::declval<RowT&>().mElements.data()) _unwrapRow(TableT& t) {
    if constexpr(TableOperations::hasRow<RowT, TableT>()) {
      return std::get<RowT>(t.mRows).mElements.data();
    }
    else {
      return nullptr;
    }
  }

  template<class TableT>
  ispc::UniformContactConstraintPairData _unwrapUniformConstraintData(TableT& constraints) {
    return {
      _unwrapRow<ConstraintData::LinearAxisX>(constraints),
      _unwrapRow<ConstraintData::LinearAxisY>(constraints),
      _unwrapRow<ConstraintData::AngularAxisOneA>(constraints),
      _unwrapRow<ConstraintData::AngularAxisOneB>(constraints),
      _unwrapRow<ConstraintData::AngularAxisTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularAxisTwoB>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisOneA>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisOneB>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisTwoB>(constraints),
      _unwrapRow<ConstraintData::ConstraintMassOne>(constraints),
      _unwrapRow<ConstraintData::ConstraintMassTwo>(constraints),
      _unwrapRow<ConstraintData::FrictionConstraintMassOne>(constraints),
      _unwrapRow<ConstraintData::FrictionConstraintMassTwo>(constraints),
      _unwrapRow<ConstraintData::LinearImpulseX>(constraints),
      _unwrapRow<ConstraintData::LinearImpulseY>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseOneA>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseOneB>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseTwoB>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseOneA>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseOneB>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseTwoA>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseTwoB>(constraints),
      _unwrapRow<ConstraintData::BiasOne>(constraints),
      _unwrapRow<ConstraintData::BiasTwo>(constraints)
    };
  }

  template<class CObj>
  ispc::UniformConstraintObject _unwrapUniformConstraintObject(ConstraintCommonTable& constraints) {
    using ConstraintT = ConstraintObject<CObj>;
    return {
      _unwrapRow<ConstraintT::LinVelX>(constraints),
      _unwrapRow<ConstraintT::LinVelY>(constraints),
      _unwrapRow<ConstraintT::AngVel>(constraints),
      _unwrapRow<ConstraintT::SyncIndex>(constraints),
      _unwrapRow<ConstraintT::SyncType>(constraints)
    };
  }
}

void Physics::details::_integratePositionAxis(float* velocity, float* position, size_t count) {
  ispc::integratePosition(position, velocity, uint32_t(count));
}

void Physics::details::_integrateRotation(float* rotX, float* rotY, float* velocity, size_t count) {
  ispc::integrateRotation(rotX, rotY, velocity, uint32_t(count));
}

void Physics::details::_applyDampingMultiplier(float* velocity, float amount, size_t count) {
  ispc::applyDampingMultiplier(velocity, amount, uint32_t(count));
}

//Hack copy of the ispc code that can be used for debugging
namespace notispc {
  float crossProduct(float ax, float ay, float bx, float by) {
    //[ax] x [bx] = [ax*by - ay*bx]
    //[ay]   [by]
    return ax*by - ay*bx;
  }

  glm::vec2 orthogonal(float x, float y) {
    //Cross product with unit Z since everything in 2D is orthogonal to Z
    //[x] [0] [ y]
    //[y]x[0]=[-x]
    //[0] [1] [ 0]
    return { y, -x };
  }

  float dotProduct(float ax, float ay, float bx, float by) {
    return ax*bx + ay*by;
  }

  float dotProduct2(glm::vec2 l, glm::vec2 r) {
    return glm::dot(l, r);
  }

  float clamp(float v, float min, float max) {
    return glm::clamp(v, min, max);
  }

  void solveContactConstraints(
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

  void solveContactConstraintsBZeroMass(
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
}

void Physics::generateContacts(CollisionPairsTable& pairs) {
  ispc::UniformConstVec2 positionsA{ _unwrapRow<NarrowphaseData<PairA>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairA>::PosY>(pairs) };
  ispc::UniformRotation rotationsA{ _unwrapRow<NarrowphaseData<PairA>::CosAngle>(pairs), _unwrapRow<NarrowphaseData<PairA>::SinAngle>(pairs) };
  ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  ispc::UniformRotation rotationsB{ _unwrapRow<NarrowphaseData<PairB>::CosAngle>(pairs), _unwrapRow<NarrowphaseData<PairB>::SinAngle>(pairs) };
  ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };

  ispc::UniformContact contactsOne{
    _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  };
  ispc::UniformContact contactsTwo{
    _unwrapRow<ContactPoint<ContactTwo>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactTwo>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactTwo>::Overlap>(pairs)
  };
  ispc::generateUnitCubeCubeContacts(positionsA, rotationsA, positionsB, rotationsB, normals, contactsOne, contactsTwo, uint32_t(TableOperations::size(pairs)));
  //ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));

  //ispc::UniformConstVec2 positionsA{ _unwrapRow<NarrowphaseData<PairA>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairA>::PosY>(pairs) };
  //ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  //ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };
  //ispc::UniformContact contacts{
  //  _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
  //  _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
  //  _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  //};
  //ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));
}

void Physics::setupConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts) {
  //Currently computing as square
  //const float pi = 3.14159265359f;
  //const float r = 0.5f;
  //const float density = 1.0f;
  //const float mass = pi*r*r;
  //const float inertia = pi*(r*r*r*r)*0.25f;
  const float w = 1.0f;
  const float h = 1.0f;
  const float mass = w*h;
  const float inertia = mass*(h*h + w*w)/12.0f;
  const float invMass = 1.0f/mass;
  const float invInertia = 1.0f/inertia;
  const float bias = 0.1f;
  ispc::UniformVec2 normal{ _unwrapRow<SharedNormal::X>(constraints), _unwrapRow<SharedNormal::Y>(constraints) };
  ispc::UniformVec2 aToContactOne{ _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneY>(constraints) };
  ispc::UniformVec2 bToContactOne{ _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactOneX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactOneY>(constraints) };
  ispc::UniformVec2 aToContactTwo{ _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(constraints) };
  ispc::UniformVec2 bToContactTwo{ _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoY>(constraints) };
  float* overlapOne = _unwrapRow<ContactPoint<ContactOne>::Overlap>(constraints);
  float* overlapTwo = _unwrapRow<ContactPoint<ContactTwo>::Overlap>(constraints);
  ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(constraints);

  //TODO: don't clear this here and use it for warm start
  float* sumsOne = _unwrapRow<ConstraintData::LambdaSumOne>(constraints);
  float* sumsTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(constraints);
  float* frictionSumsOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(constraints);
  float* frictionSumsTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(constraints);
  for(size_t i = 0; i < TableOperations::size(constraints); ++i) {
    sumsOne[i] = sumsTwo[i] = frictionSumsOne[i] = frictionSumsTwo[i] = 0.0f;
  }

  ispc::setupConstraintsSharedMass(invMass, invInertia, bias, normal, aToContactOne, aToContactTwo, bToContactOne, bToContactTwo, overlapOne, overlapTwo, data, uint32_t(TableOperations::size(constraints)));

  normal = { _unwrapRow<SharedNormal::X>(staticContacts), _unwrapRow<SharedNormal::Y>(staticContacts) };
  aToContactOne = { _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneX>(staticContacts), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneY>(staticContacts) };
  aToContactTwo = { _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(staticContacts), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(staticContacts) };
  overlapOne = _unwrapRow<ContactPoint<ContactOne>::Overlap>(staticContacts);
  overlapTwo = _unwrapRow<ContactPoint<ContactTwo>::Overlap>(staticContacts);
  data = _unwrapUniformConstraintData(staticContacts);

  sumsOne = _unwrapRow<ConstraintData::LambdaSumOne>(staticContacts);
  sumsTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(staticContacts);
  frictionSumsOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(staticContacts);
  frictionSumsTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(staticContacts);
  for(size_t i = 0; i < TableOperations::size(staticContacts); ++i) {
    sumsOne[i] = sumsTwo[i] = frictionSumsOne[i] = frictionSumsTwo[i] = 0.0f;
  }

  ispc::setupConstraintsSharedMassBZeroMass(invMass, invInertia, bias, normal, aToContactOne, aToContactTwo, overlapOne, overlapTwo, data, uint32_t(TableOperations::size(staticContacts)));
}

void Physics::solveConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts, ConstraintCommonTable& common, const PhysicsConfig& config) {
  ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(constraints);
  ispc::UniformConstraintObject objectA = _unwrapUniformConstraintObject<ConstraintObjA>(common);
  ispc::UniformConstraintObject objectB = _unwrapUniformConstraintObject<ConstraintObjB>(common);
  float* lambdaSumOne = _unwrapRow<ConstraintData::LambdaSumOne>(constraints);
  float* lambdaSumTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(constraints);
  float* frictionLambdaSumOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(constraints);
  float* frictionLambdaSumTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(constraints);
  uint8_t* enabled = _unwrapRow<ConstraintData::IsEnabled>(common);

  const float frictionCoeff = 0.5f;
  const size_t startContact = std::get<ConstraintData::CommonTableStartIndex>(constraints.mRows).at();
  const size_t startStatic = std::get<ConstraintData::CommonTableStartIndex>(staticContacts.mRows).at();

  const bool oneAtATime = config.mForcedTargetWidth && *config.mForcedTargetWidth < ispc::getTargetWidth();

  if(oneAtATime) {
    for(size_t i = 0; i < TableOperations::size(constraints); ++i) {
      ispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startContact), uint32_t(i), uint32_t(1));
    }
  }
  else {
    ispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startContact), uint32_t(0), uint32_t(TableOperations::size(constraints)));
  }

  data = _unwrapUniformConstraintData(staticContacts);
  lambdaSumOne = _unwrapRow<ConstraintData::LambdaSumOne>(staticContacts);
  lambdaSumTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(staticContacts);
  frictionLambdaSumOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(staticContacts);
  frictionLambdaSumTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(staticContacts);

  if(oneAtATime) {
    for(size_t i = 0; i < TableOperations::size(staticContacts); ++i) {
      ispc::solveContactConstraintsBZeroMass(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startStatic), uint32_t(i), uint32_t(1));
    }
  }
  else {
    ispc::solveContactConstraintsBZeroMass(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startStatic), uint32_t(0), uint32_t(TableOperations::size(staticContacts)));
  }
}