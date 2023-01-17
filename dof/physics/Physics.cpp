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
    float frictionCoeff,
    uint32_t objectOffset,
    uint32_t start,
    uint32_t count
  ) {
    for(int t = 0; t < (int)count; ++t) {
      const int i = t + start;
      const int oi = i + objectOffset;
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

template<class PosRowA, class PosRowB, class ContactRow, class DstRowA, class DstRowB>
void _toRVector(CollisionPairsTable& pairs, ConstraintsTable& constraints) {
  PosRowA& posA = std::get<PosRowA>(pairs.mRows);
  PosRowB& posB = std::get<PosRowB>(pairs.mRows);
  ContactRow& contacts = std::get<ContactRow>(pairs.mRows);
  DstRowA& dstA = std::get<DstRowA>(constraints.mRows);
  DstRowB& dstB = std::get<DstRowB>(constraints.mRows);

  ispc::turnContactsToRVectors(
    posA.mElements.data(),
    posB.mElements.data(),
    contacts.mElements.data(),
    dstA.mElements.data(),
    dstB.mElements.data(),
    uint32_t(TableOperations::size(pairs)));
}

struct ConstraintSyncData {
  template<class T, class TableT>
  T* _tryGetRow(TableT& table) {
    if constexpr(TableOperations::hasRow<T, TableT>()) {
      return &std::get<T>(table.mRows);
    }
    else {
      return nullptr;
    }
  }

  template<class TableT>
  ConstraintSyncData(CollisionPairsTable& pairs, ConstraintCommonTable& common, TableT& constraints)
    : mSyncIndexA(std::get<ConstraintObject<ConstraintObjA>::SyncIndex>(common.mRows))
    , mSyncTypeA(std::get<ConstraintObject<ConstraintObjA>::SyncType>(common.mRows))
    , mPairIndexA(std::get<CollisionPairIndexA>(common.mRows))
    , mCenterToContactOneXA(_tryGetRow<ConstraintObject<ConstraintObjA>::CenterToContactOneX>(constraints))
    , mCenterToContactOneYA(_tryGetRow<ConstraintObject<ConstraintObjA>::CenterToContactOneY>(constraints))
    , mCenterToContactTwoXA(_tryGetRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(constraints))
    , mCenterToContactTwoYA(_tryGetRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(constraints))
    , mSyncIndexB(std::get<ConstraintObject<ConstraintObjB>::SyncIndex>(common.mRows))
    , mSyncTypeB(std::get<ConstraintObject<ConstraintObjB>::SyncType>(common.mRows))
    , mPairIndexB(std::get<CollisionPairIndexB>(common.mRows))
    , mCenterToContactOneXB(_tryGetRow<ConstraintObject<ConstraintObjB>::CenterToContactOneX>(constraints))
    , mCenterToContactOneYB(_tryGetRow<ConstraintObject<ConstraintObjB>::CenterToContactOneY>(constraints))
    , mCenterToContactTwoXB(_tryGetRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoX>(constraints))
    , mCenterToContactTwoYB(_tryGetRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoY>(constraints))
    , mDestContactOneOverlap(_tryGetRow<ContactPoint<ContactOne>::Overlap>(constraints))
    , mDestContactTwoOverlap(_tryGetRow<ContactPoint<ContactTwo>::Overlap>(constraints))
    , mDestNormalX(_tryGetRow<SharedNormal::X>(constraints))
    , mDestNormalY(_tryGetRow<SharedNormal::Y>(constraints))
    , mContactOneX(_tryGetRow<ContactPoint<ContactOne>::PosX>(pairs))
    , mContactOneY(_tryGetRow<ContactPoint<ContactOne>::PosY>(pairs))
    , mContactTwoX(_tryGetRow<ContactPoint<ContactTwo>::PosX>(pairs))
    , mContactTwoY(_tryGetRow<ContactPoint<ContactTwo>::PosY>(pairs))
    , mPosAX(_tryGetRow<NarrowphaseData<PairA>::PosX>(pairs))
    , mPosAY(_tryGetRow<NarrowphaseData<PairA>::PosY>(pairs))
    , mPosBX(_tryGetRow<NarrowphaseData<PairB>::PosX>(pairs))
    , mPosBY(_tryGetRow<NarrowphaseData<PairB>::PosY>(pairs))
    , mSourceContactOneOverlap(_tryGetRow<ContactPoint<ContactOne>::Overlap>(pairs))
    , mSourceContactTwoOverlap(_tryGetRow<ContactPoint<ContactTwo>::Overlap>(pairs))
    , mSourceNormalX(_tryGetRow<SharedNormal::X>(pairs))
    , mSourceNormalY(_tryGetRow<SharedNormal::Y>(pairs)) {
  }

  ConstraintObject<ConstraintObjA>::SyncIndex& mSyncIndexA;
  ConstraintObject<ConstraintObjA>::SyncType& mSyncTypeA;
  CollisionPairIndexA& mPairIndexA;
  ConstraintObject<ConstraintObjB>::SyncIndex& mSyncIndexB;
  ConstraintObject<ConstraintObjB>::SyncType& mSyncTypeB;
  CollisionPairIndexB& mPairIndexB;

  ConstraintObject<ConstraintObjA>::CenterToContactOneX* mCenterToContactOneXA{};
  ConstraintObject<ConstraintObjA>::CenterToContactOneY* mCenterToContactOneYA{};
  ConstraintObject<ConstraintObjA>::CenterToContactTwoX* mCenterToContactTwoXA{};
  ConstraintObject<ConstraintObjA>::CenterToContactTwoY* mCenterToContactTwoYA{};

  ConstraintObject<ConstraintObjB>::CenterToContactOneX* mCenterToContactOneXB{};
  ConstraintObject<ConstraintObjB>::CenterToContactOneY* mCenterToContactOneYB{};
  ConstraintObject<ConstraintObjB>::CenterToContactTwoX* mCenterToContactTwoXB{};
  ConstraintObject<ConstraintObjB>::CenterToContactTwoY* mCenterToContactTwoYB{};

  ContactPoint<ContactOne>::Overlap* mDestContactOneOverlap{};
  ContactPoint<ContactTwo>::Overlap* mDestContactTwoOverlap{};
  SharedNormal::X* mDestNormalX{};
  SharedNormal::Y* mDestNormalY{};

  ContactPoint<ContactOne>::PosX* mContactOneX{};
  ContactPoint<ContactOne>::PosY* mContactOneY{};
  ContactPoint<ContactTwo>::PosX* mContactTwoX{};
  ContactPoint<ContactTwo>::PosY* mContactTwoY{};

  NarrowphaseData<PairA>::PosX* mPosAX{};
  NarrowphaseData<PairA>::PosY* mPosAY{};
  NarrowphaseData<PairB>::PosX* mPosBX{};
  NarrowphaseData<PairB>::PosY* mPosBY{};

  ContactPoint<ContactOne>::Overlap* mSourceContactOneOverlap{};
  ContactPoint<ContactTwo>::Overlap* mSourceContactTwoOverlap{};
  SharedNormal::X* mSourceNormalX{};
  SharedNormal::Y* mSourceNormalY{};
};

void _syncConstraintData(ConstraintSyncData& data, size_t globalConstraintIndex, size_t constraintIndex, size_t objectIndex, const StableElementID& indexA, const StableElementID& indexB) {
  data.mPairIndexA.at(globalConstraintIndex) = indexA;

  //Assume that if one row of A exist they all do
  if(data.mCenterToContactOneXA) {
    data.mCenterToContactOneXA->at(constraintIndex) = data.mContactOneX->at(objectIndex) - data.mPosAX->at(objectIndex);
    data.mCenterToContactOneYA->at(constraintIndex) = data.mContactOneY->at(objectIndex) - data.mPosAY->at(objectIndex);
    data.mCenterToContactTwoXA->at(constraintIndex) = data.mContactTwoX->at(objectIndex) - data.mPosAX->at(objectIndex);
    data.mCenterToContactTwoYA->at(constraintIndex) = data.mContactTwoY->at(objectIndex) - data.mPosAY->at(objectIndex);
  }

  data.mDestContactOneOverlap->at(constraintIndex) = data.mSourceContactOneOverlap->at(objectIndex);
  data.mDestContactTwoOverlap->at(constraintIndex) = data.mSourceContactTwoOverlap->at(objectIndex);
  data.mDestNormalX->at(constraintIndex) = data.mSourceNormalX->at(objectIndex);
  data.mDestNormalY->at(constraintIndex) = data.mSourceNormalY->at(objectIndex);

  data.mPairIndexB.at(globalConstraintIndex) = indexB;
  if(data.mCenterToContactOneXB) {
    data.mCenterToContactOneXB->at(constraintIndex) = data.mContactOneX->at(objectIndex) - data.mPosBX->at(objectIndex);
    data.mCenterToContactOneYB->at(constraintIndex) = data.mContactOneY->at(objectIndex) - data.mPosBY->at(objectIndex);
    data.mCenterToContactTwoXB->at(constraintIndex) = data.mContactTwoX->at(objectIndex) - data.mPosBX->at(objectIndex);
    data.mCenterToContactTwoYB->at(constraintIndex) = data.mContactTwoY->at(objectIndex) - data.mPosBY->at(objectIndex);
  }
}

struct VisitAttempt {
  StableElementID mDesiredObjectIndex{};
  size_t mDesiredConstraintIndex{};
  std::vector<ConstraintData::VisitData>::iterator mIt;
  size_t mRequiredPadding{};
};

void _setVisitDataAndTrySetSyncPoint(std::vector<ConstraintData::VisitData>& visited, VisitAttempt& attempt,
  ConstraintObject<ConstraintObjA>::SyncIndex& syncIndexA,
  ConstraintObject<ConstraintObjA>::SyncType& syncTypeA,
  ConstraintObject<ConstraintObjB>::SyncIndex& syncIndexB,
  ConstraintObject<ConstraintObjB>::SyncType& syncTypeB,
  ConstraintData::VisitData::Location location,
  VisitAttempt* dependentAttempt) {
  //Set it to nosync for now, later iteration might set this as new constraints are visited
  syncTypeA.at(attempt.mDesiredConstraintIndex) = ispc::NoSync;
  syncTypeB.at(attempt.mDesiredConstraintIndex) = ispc::NoSync;
  if(attempt.mIt == visited.end() || attempt.mIt->mObjectIndex != attempt.mDesiredObjectIndex) {
    //Goofy hack here, if there's another iterator for object B, make sure the iterator is still valid after the new element is inserted
    //Needs to be rebuilt if it is after the insert location, as everything would have shifted over
    const bool needRebuiltDependent = dependentAttempt && dependentAttempt->mDesiredObjectIndex.mStableID > attempt.mDesiredObjectIndex.mStableID;

    //If this is the first time visiting this object, no need to sync anything, but note it for later
    visited.insert(attempt.mIt, ConstraintData::VisitData{
      attempt.mDesiredObjectIndex,
      attempt.mDesiredConstraintIndex,
      location,
      attempt.mDesiredConstraintIndex,
      location
    });

    if(needRebuiltDependent) {
      dependentAttempt->mIt = std::lower_bound(visited.begin(), visited.end(), dependentAttempt->mDesiredObjectIndex);
    }
  }
  else {
    //A has been visited before, add a sync index
    const int newLocation = location == ConstraintData::VisitData::Location::InA ? ispc::SyncToIndexA : ispc::SyncToIndexB;
    //Make the previously visited constraint publish the velocity forward to this one
    switch(attempt.mIt->mLocation) {
      case ConstraintData::VisitData::Location::InA:
        syncTypeA.at(attempt.mIt->mConstraintIndex) = newLocation;
        syncIndexA.at(attempt.mIt->mConstraintIndex) = attempt.mDesiredConstraintIndex;
        break;
      case ConstraintData::VisitData::Location::InB:
        syncTypeB.at(attempt.mIt->mConstraintIndex) = newLocation;
        syncIndexB.at(attempt.mIt->mConstraintIndex) = attempt.mDesiredConstraintIndex;
        break;
    }
    //Now that the latest instance of this object is at this visit location, update the visit data
    attempt.mIt->mConstraintIndex = attempt.mDesiredConstraintIndex;
    attempt.mIt->mLocation = location;
  }
}

//If an object shows up in multiple constraints, sync the velocity data from the last constraint back to the first
void _trySetFinalSyncPoint(const ConstraintData::VisitData& visited,
  ConstraintObject<ConstraintObjA>::SyncIndex& syncIndexA,
  ConstraintObject<ConstraintObjA>::SyncType& syncTypeA,
  ConstraintObject<ConstraintObjB>::SyncIndex& syncIndexB,
  ConstraintObject<ConstraintObjB>::SyncType& syncTypeB) {
  //If the first is the latest, there is only one instance of this object meaning its velocity was never copied
  if(visited.mFirstConstraintIndex == visited.mConstraintIndex) {
    return;
  }

  //The container to sync from, meaning the final visited entry where the most recent velocity is
  std::vector<int>* syncFromIndex = &syncIndexA.mElements;
  std::vector<int>* syncFromType = &syncTypeA.mElements;
  if(visited.mLocation == ConstraintData::VisitData::Location::InB) {
    syncFromIndex = &syncIndexB.mElements;
    syncFromType = &syncTypeB.mElements;
  }

  //Sync from this visited entry back to the first element
  syncFromIndex->at(visited.mConstraintIndex) = visited.mFirstConstraintIndex;
  //Write from the location of the final entry to the location of the first
  switch(visited.mFirstLocation) {
    case ConstraintData::VisitData::Location::InA: syncFromType->at(visited.mConstraintIndex) = ispc::SyncToIndexA; break;
    case ConstraintData::VisitData::Location::InB: syncFromType->at(visited.mConstraintIndex) = ispc::SyncToIndexB; break;
  }
}

VisitAttempt _tryVisit(std::vector<ConstraintData::VisitData>& visited, const StableElementID& toVisit, size_t currentConstraintIndex, size_t targetWidth) {
  VisitAttempt result;
  result.mIt = std::lower_bound(visited.begin(), visited.end(), toVisit);
  if(result.mIt != visited.end() && result.mIt->mObjectIndex == toVisit && 
    (currentConstraintIndex - result.mIt->mConstraintIndex) < targetWidth) {
    result.mRequiredPadding = targetWidth - (currentConstraintIndex - result.mIt->mConstraintIndex);
  }
  result.mDesiredConstraintIndex = currentConstraintIndex;
  result.mDesiredObjectIndex = toVisit;
  return result;
}

//TODO: what if all the constraints were block solved? If they were solved in blocks as wide as simd lanes then
//maybe this complicated shuffling wouldn't be needed. Or maybe even a giant matrix for all objects

void Physics::buildConstraintsTable(
  CollisionPairsTable& pairs,
  ConstraintsTable& constraints,
  ContactConstraintsToStaticObjectsTable& staticConstraints,
  ConstraintCommonTable& constraintsCommon,
  const PhysicsTableIds& tableIds,
  const PhysicsConfig& config) {
  const size_t tableIDMask = tableIds.mTableIDMask;
  const size_t sharedMassTableId = tableIds.mSharedMassTable;
  const size_t zeroMassTableId = tableIds.mZeroMassTable;

  //TODO: only rebuild if collision pairs changed. Could be really lazy about it since solving stale constraints should result in no changes to velocity
  //Clear before resize. Shouldn't be necessary but there seems to be a bug somewhere
  TableOperations::resizeTable(constraints, 0);
  TableOperations::resizeTable(staticConstraints, 0);
  TableOperations::resizeTable(constraintsCommon, 0);
  TableOperations::resizeTable(constraints, TableOperations::size(pairs));
  TableOperations::resizeTable(staticConstraints, TableOperations::size(pairs));
  TableOperations::resizeTable(constraintsCommon, TableOperations::size(pairs));

  ConstraintData::SharedVisitData& visitData = std::get<ConstraintData::SharedVisitDataRow>(constraintsCommon.mRows).at();
  std::vector<ConstraintData::VisitData>& visited = visitData.mVisited;
  visited.clear();
  //Need to reserve big enough because visited vector growth would cause iterator invalidation for visitA/visitB cases below
  //Size of pairs is bigger than necessary, it only needs to be the number of objects, but that's not known here and over-allocating
  //a bit isn't a problem
  visited.reserve(TableOperations::size(pairs));
  ConstraintSyncData data(pairs, constraintsCommon, constraints);

  //Specifically from the collision table not the constraints table, to be used before the indices in the collision table have been created yet
  auto& srcPairIndexA = std::get<CollisionPairIndexA>(pairs.mRows);
  auto& srcPairIndexB = std::get<CollisionPairIndexB>(pairs.mRows);

  //The same object can't be within this many indices of itself since the velocity needs to be seen immediately by the next constraint
  //which wouldn't be the case if it was being solved in another simd lane
  const size_t targetWidth = config.mForcedTargetWidth.value_or(size_t(ispc::getTargetWidth()));
  //Figure out sync indices
  std::deque<size_t>& indicesToFill = visitData.mIndicesToFill;
  std::deque<size_t>& nextToFill = visitData.mNextToFill;
  nextToFill.clear();
  indicesToFill.resize(TableOperations::size(pairs));
  for(size_t i = 0; i < TableOperations::size(pairs); ++i) {
    indicesToFill[i] = i;
  }
  size_t currentConstraintIndex = 0;
  size_t failedPlacements = 0;
  //First fill shared mass pairs
  //Fill each element in the constraints table one by one, trying the latest in indicesToFill each type
  //If it fails, swap it to back and hope it works later. If it doesn't work this will break out with remaining indices
  const size_t sharedMassStart = currentConstraintIndex;
  std::get<ConstraintData::CommonTableStartIndex>(constraints.mRows).at() = sharedMassStart;
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    indicesToFill.pop_front();
    const StableElementID desiredA = srcPairIndexA.at(indexToFill);
    const StableElementID desiredB = srcPairIndexB.at(indexToFill);
    //This loop is for shared mass, store the rest for the next pass
    //TODO: this is a bit weird. != shared mass would be nice but that means players get excluded
    if((desiredA.mUnstableIndex & tableIDMask) == zeroMassTableId || (desiredB.mUnstableIndex & tableIDMask) == zeroMassTableId) {
      nextToFill.push_back(indexToFill);
      continue;
    }

    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    VisitAttempt visitB = _tryVisit(visited, desiredB, currentConstraintIndex, targetWidth);

    //If this can't be filled now keep going and hopefully this will work later
    if(visitA.mRequiredPadding || visitB.mRequiredPadding) {
      indicesToFill.push_back(indexToFill);
      //If the last few are impossible to fill this way, break
      if(++failedPlacements >= indicesToFill.size()) {
        break;
      }
      continue;
    }

    _syncConstraintData(data, currentConstraintIndex, currentConstraintIndex - sharedMassStart, indexToFill, desiredA, desiredB);

    _setVisitDataAndTrySetSyncPoint(visited, visitA, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InA, &visitB);
    _setVisitDataAndTrySetSyncPoint(visited, visitB, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InB, nullptr);

    ++currentConstraintIndex;
    failedPlacements = 0;
  }

  //If elements remain here it means there wasn't an order of elements possible as-is.
  //Add padding between these remaining elements to solve the problem.
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    const StableElementID desiredA = srcPairIndexA.at(indexToFill);
    const StableElementID desiredB = srcPairIndexB.at(indexToFill);
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    VisitAttempt visitB = _tryVisit(visited, desiredB, currentConstraintIndex, targetWidth);
    const size_t padding = std::max(visitA.mRequiredPadding, visitB.mRequiredPadding);

    //If no padding is required that means the previous iteration made space, add the constraint here
    if(!padding) {
      _syncConstraintData(data, currentConstraintIndex, currentConstraintIndex - sharedMassStart, indexToFill, desiredA, desiredB);
      _setVisitDataAndTrySetSyncPoint(visited, visitA, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InA, &visitB);
      _setVisitDataAndTrySetSyncPoint(visited, visitB, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InB, nullptr);
      indicesToFill.pop_front();
      ++currentConstraintIndex;
    }
    //Space needs to be made, add padding then let the iteration contine which will attempt the index again with the new space
    else {
      TableOperations::resizeTable(constraints, TableOperations::size(constraints) + padding);
      TableOperations::resizeTable(constraintsCommon, TableOperations::size(constraintsCommon) + padding);
      for(size_t i = 0; i < padding; ++i) {
        //These are nonsense entires that won't be used for anything, at least set them to no sync so they don't try to copy stale data
        data.mSyncTypeA.at(currentConstraintIndex) = ispc::NoSync;
        data.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;
        data.mPairIndexA.at(currentConstraintIndex) = StableElementID::invalid();
        data.mPairIndexB.at(currentConstraintIndex) = StableElementID::invalid();
        ++currentConstraintIndex;
      }
    }
  }
  //Trim off unused entries
  TableOperations::resizeTable(constraints, currentConstraintIndex - sharedMassStart);

  //Fill contacts with static objects
  const size_t zeroMassStart = currentConstraintIndex;
  std::get<ConstraintData::CommonTableStartIndex>(staticConstraints.mRows).at() = zeroMassStart;
  ConstraintSyncData staticData(pairs, constraintsCommon, staticConstraints);
  indicesToFill = nextToFill;
  nextToFill.clear();
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    indicesToFill.pop_front();
    StableElementID desiredA = srcPairIndexA.at(indexToFill);
    StableElementID desiredB = srcPairIndexB.at(indexToFill);
    //This loop is for zero mass, store the rest for the next pass
    const bool aZeroMass = (desiredA.mUnstableIndex & tableIDMask) == zeroMassTableId;
    const bool bZeroMass = (desiredB.mUnstableIndex & tableIDMask) == zeroMassTableId;
    //If neither is zero mass, push them to the next phase
    if(!aZeroMass && !bZeroMass) {
      nextToFill.push_back(indexToFill);
      continue;
    }
    assert(!aZeroMass && "Pair generation should have ensured zero mass object is always B");

    //No visit of static B needed because no velocity syncing is needed
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);

    //If this can't be filled now keep going and hopefully this will work later
    //No padding needed for B because its velocity will always be zero
    if(visitA.mRequiredPadding) {
      indicesToFill.push_back(indexToFill);
      //If the last few are impossible to fill this way, break
      if(++failedPlacements >= indicesToFill.size()) {
        break;
      }
      continue;
    }

    _syncConstraintData(staticData, currentConstraintIndex, currentConstraintIndex - zeroMassStart, indexToFill, desiredA, desiredB);

    _setVisitDataAndTrySetSyncPoint(visited, visitA, staticData.mSyncIndexA, staticData.mSyncTypeA, staticData.mSyncIndexB, staticData.mSyncTypeB, ConstraintData::VisitData::Location::InA, nullptr);
    staticData.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;

    ++currentConstraintIndex;
    failedPlacements = 0;
  }

  assert(nextToFill.empty() && "No other categories implemented");

  //If elements remain here it means there wasn't an order of elements possible as-is.
  //Add padding between these remaining elements to solve the problem.
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    const StableElementID desiredA = srcPairIndexA.at(indexToFill);
    const StableElementID desiredB = srcPairIndexB.at(indexToFill);
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    const size_t padding = visitA.mRequiredPadding;

    //If no padding is required that means the previous iteration made space, add the constraint here
    if(!padding) {
      _syncConstraintData(staticData, currentConstraintIndex, currentConstraintIndex - zeroMassStart, indexToFill, desiredA, desiredB);
      _setVisitDataAndTrySetSyncPoint(visited, visitA, staticData.mSyncIndexA, staticData.mSyncTypeA, staticData.mSyncIndexB, staticData.mSyncTypeB, ConstraintData::VisitData::Location::InA, nullptr);
      staticData.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;
      indicesToFill.pop_front();
      ++currentConstraintIndex;
    }
    //Space needs to be made, add padding then let the iteration contine which will attempt the index again with the new space
    else {
      TableOperations::resizeTable(staticConstraints, TableOperations::size(staticConstraints) + padding);
      TableOperations::resizeTable(constraintsCommon, TableOperations::size(constraintsCommon) + padding);
      for(size_t i = 0; i < padding; ++i) {
        //These are nonsense entires that won't be used for anything, at least set them to no sync so they don't try to copy stale data
        staticData.mSyncTypeA.at(currentConstraintIndex) = ispc::NoSync;
        staticData.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;
        data.mPairIndexA.at(currentConstraintIndex) = StableElementID::invalid();
        data.mPairIndexB.at(currentConstraintIndex) = StableElementID::invalid();
        ++currentConstraintIndex;
      }
    }
  }
  TableOperations::resizeTable(staticConstraints, currentConstraintIndex - zeroMassStart);
  //Trim everything else off the end
  TableOperations::resizeTable(constraintsCommon, currentConstraintIndex);

  //Store the final indices that the velocity will end up in
  //This is in the visited data since that's been tracking every access
  FinalSyncIndices& finalData = std::get<SharedRow<FinalSyncIndices>>(constraintsCommon.mRows).at();
  finalData.mMappingsA.clear();
  finalData.mMappingsB.clear();
  for(const ConstraintData::VisitData& v : visited) {
    //Link the final constraint entry back to the velocity data of the first that uses the objects if velocity was duplicated
    //This matters from one iteration to the next to avoid working on stale data, doesn't matter for the last iteration
    //since the final results will be copied from the end locations stored in mappings below
    _trySetFinalSyncPoint(v, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB);

    //Store the final location mapping
    switch(v.mLocation) {
      case ConstraintData::VisitData::Location::InA: {
        finalData.mMappingsA.emplace_back(FinalSyncIndices::Mapping{ v.mObjectIndex, v.mConstraintIndex });
        break;
      }
      case ConstraintData::VisitData::Location::InB: {
        finalData.mMappingsB.emplace_back(FinalSyncIndices::Mapping{ v.mObjectIndex, v.mConstraintIndex });
        break;
      }
    }
  }
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

  const float frictionCoeff = 0.5f;
  const size_t startContact = std::get<ConstraintData::CommonTableStartIndex>(constraints.mRows).at();
  const size_t startStatic = std::get<ConstraintData::CommonTableStartIndex>(staticContacts.mRows).at();

  const bool oneAtATime = config.mForcedTargetWidth && *config.mForcedTargetWidth < ispc::getTargetWidth();

  if(oneAtATime) {
    for(size_t i = 0; i < TableOperations::size(constraints); ++i) {
      notispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, frictionCoeff, uint32_t(startContact), uint32_t(i), uint32_t(1));
    }
  }
  else {
    ispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, frictionCoeff, uint32_t(startContact), uint32_t(0), uint32_t(TableOperations::size(constraints)));
  }

  data = _unwrapUniformConstraintData(staticContacts);
  lambdaSumOne = _unwrapRow<ConstraintData::LambdaSumOne>(staticContacts);
  lambdaSumTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(staticContacts);
  frictionLambdaSumOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(staticContacts);
  frictionLambdaSumTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(staticContacts);

  if(oneAtATime) {
    for(size_t i = 0; i < TableOperations::size(staticContacts); ++i) {
      notispc::solveContactConstraintsBZeroMass(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, frictionCoeff, uint32_t(startStatic), uint32_t(i), uint32_t(1));
    }
  }
  else {
    ispc::solveContactConstraintsBZeroMass(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, frictionCoeff, uint32_t(startStatic), uint32_t(0), uint32_t(TableOperations::size(staticContacts)));
  }
}