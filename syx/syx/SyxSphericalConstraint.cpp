#include "Precompile.h"
#include "SyxSphericalConstraint.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"
#include "SyxPhysicsObject.h"

namespace Syx {
    const float LocalSphericalConstraint::sLinearSlop = 0.05f;
    const float LocalSphericalConstraint::sAngularSlop = 0.05f;

    static Vec3 testA;
    static Vec3 testB;

    //Cross vector with cardinal axis indicated by index, 0=x,1=y,2=z
    static Vec3 CrossCardinal(const Vec3& v, unsigned int axis) {
      switch(axis) {
        case 0: return Vec3(0.0f, v.z, -v.y);
        case 1: return Vec3(-v.z, 0.0f, v.x);
        case 2: return Vec3(v.y, -v.x, 0.0f);
        default: SyxAssertError(false, "Invalid index"); return Vec3();
      }
    }

    void SphericalConstraint::SetSwingFrame(const Quat& aFrame) {
      mRefA = aFrame;
      //Transform reference A to world then into local b
      mRefB = mB->GetTransform().mRot.Inversed() * mA->GetTransform().mRot * mRefA;
    }

    void SphericalConstraint::GetAngularReferences(Quat& ra, Quat& rb) const {
      ra = mRefA;
      rb = mRefB;
    }

    void SphericalConstraint::GetSwingLimits(float& maxRadsX, float& maxRadsY) const {
      maxRadsX = mMaxSwingRadsX;
      maxRadsY = mMaxSwingRadsY;
    }

    void SphericalConstraint::SetSwingLimits(float maxRadsX, float maxRadsY) {
      mMaxSwingRadsX = maxRadsX;
      mMaxSwingRadsY = maxRadsY;
    }

    void SphericalConstraint::GetTwistLimits(float& minRads, float& maxRads) const {
      minRads = mMinTwistRads;
      maxRads = mMaxTwistRads;
    }

    void SphericalConstraint::SetTwistLimits(float minRads, float maxRads) {
      mMinTwistRads = minRads;
      mMaxTwistRads = maxRads;
    }

    void SphericalConstraint::SetMaxAngularImpulse(float max) {
      mMaxAngularImpulse = max;
    }

    float SphericalConstraint::GetMaxAngularImpulse() const {
      return mMaxAngularImpulse;
    }

    void SphericalBlock::Setup(const Vec3& posA, const Vec3& posB, const Vec3& anchorA, const Vec3& anchorB, float massA, float massB, const Mat3& inertiaA, const Mat3& inertiaB) {
      Vec3 ra = anchorA - posA;
      Vec3 rb = anchorB - posB;
      Vec3 error = anchorA - anchorB;
      float halfSlop = LocalSphericalConstraint::sLinearSlop*0.5f;

      for(int i = 0; i < 3; ++i) {
        mAngularA[i] = CrossCardinal(ra, i);
        mAngularB[i] = -CrossCardinal(rb, i);

        mAngularMA[i] = inertiaA*mAngularA[i];
        mAngularMB[i] = inertiaB*mAngularB[i];

        mBias[i] = Constraints::ComputeBias(error[i], halfSlop, LocalConstraint::sVelBaumgarteTerm, LocalConstraint::sMaxVelCorrection);
      }

      mMassA = massA;
      mMassB = massB;
      mLambdaSum = Vector3::Zero;

      //J*M^1*J^T with zero terms removed due to axes being orthogonal to each other
      const Vec3& rax = mAngularA[0]; const Vec3& ray = mAngularA[1]; const Vec3& raz = mAngularA[2];
      const Vec3& rbx = mAngularB[0]; const Vec3& rby = mAngularB[1]; const Vec3& rbz = mAngularB[2];
      const Vec3& rmax = mAngularMA[0]; const Vec3& rmay = mAngularMA[1]; const Vec3& rmaz = mAngularMA[2];
      const Vec3& rmbx = mAngularMB[0]; const Vec3& rmby = mAngularMB[1]; const Vec3& rmbz = mAngularMB[2];
      float mab = mMassA + mMassB;
      //Looks like it, but is not actually symmetric all the time
      mConstraintMass = Mat3(mab + rmax.Dot(rax) + rmbx.Dot(rbx), rmax.Dot(ray) + rmbx.Dot(rby), rmax.Dot(raz) + rmbx.Dot(rbz),
        rmay.Dot(rax) + rmby.Dot(rbx), mab + rmay.Dot(ray) + rmby.Dot(rby), rmay.Dot(raz) + rmby.Dot(rbz),
        rmaz.Dot(rax) + rmbz.Dot(rbx), rmaz.Dot(ray) + rmbz.Dot(rby), mab + rmaz.Dot(raz) + rmbz.Dot(rbz));
      mConstraintMass = mConstraintMass.Inverse();
    }

    void SphericalBlock::ApplyImpulse(const Vec3& lambda, ConstraintObjBlock& a, ConstraintObjBlock& b) {
      for(int i = 0; i < 3; ++i) {
        Vec3 linearA, linearB;
        linearA = linearB = Vector3::Zero;
        linearA[i] = mMassA;
        linearB[i] = -mMassB;
        Constraints::ApplyImpulse(lambda[i], linearA, mAngularMA[i], linearB, mAngularMB[i], a, b);
      }
      mLambdaSum += lambda;
    }

    float SphericalBlock::Solve(ConstraintObjBlock& a, ConstraintObjBlock& b) {
      Vec3 jv;
      for(int i = 0; i < 3; ++i) {
        //Dot product of cardinal axis with velocity of a and b, then the angular components as usual
        jv[i] = a.mLinVel[i] - b.mLinVel[i] + a.mAngVel.Dot(mAngularA[i]) + b.mAngVel.Dot(mAngularB[i]);
      }

      Vec3 lambda = Constraints::ComputeLambda(jv, mBias, mConstraintMass);
      ApplyImpulse(lambda, a, b);
      return std::abs(lambda.x) + std::abs(lambda.y) + std::abs(lambda.z);
    }

    float SphericalBlock::SSolve(SFloats& linVelA, SFloats& angVelA, SFloats& linVelB, SFloats& angVelB) {
      SFloats jv;
      SFloats velDiff = SSubAll(linVelA, linVelB);

      SFloats jvs[3];
      for(int i = 0; i < 3; ++i) {
        jvs[i] = SVector3::Sum3(SAddAll(SMulAll(angVelA, SLoadAll(&mAngularA[i].x)), SMulAll(angVelB, SLoadAll(&mAngularB[i].x))));
        jvs[i] =  SAddAll(jvs[i], SSplatIndex(velDiff, i));
      }
      jv = SCombine(jvs[0], jvs[1], jvs[2]);

      SFloats lambda = SConstraints::ComputeLambda(jv, SLoadAll(&mBias.x), ToSMatrix3(mConstraintMass));
      //Accumulate lambda sum
      Vec3 oldSum = mLambdaSum;
      SStoreAll(&mLambdaSum.x, SAddAll(SLoadAll(&mLambdaSum.x), lambda));

      //Splat mass into two vectors, we'll mask out non relevant components to form the linear portions of the jacobian
      SAlign Vec3 masses(mMassA, mMassA, mMassB, mMassB);
      SFloats massA = SLoadAll(&masses.x);
      SFloats massB = SVector3::Neg(SShuffle(massA, 2, 2, 2, 2));
      massA = SShuffle(massA, 0, 0, 0, 0);
      for(int i = 0; i < 3; ++i) {
        SConstraints::ApplyImpulse(SSplatIndex(lambda, i), linVelA, angVelA, linVelB, angVelB,
          SMaskOtherIndices(massA, i), SLoadAll(&mAngularMA[i].x),
          SMaskOtherIndices(massB, i), SLoadAll(&mAngularMB[i].x));
      }

      lambda = SVector3::Sum3(SAbsAll(lambda));
      SStoreAll(&masses.x, lambda);
      return masses.x;
    }

    void SwingTwistBlock::Setup(const Quat& refA, const Quat& refB,
        const Quat& rotA, const Quat& rotB,
        const Mat3& inertiaA, const Mat3& inertiaB,
        float maxSwingX, float maxSwingY,
        float minTwist, float maxTwist,
        float maxAngularImpulse) {
      bool limitSwing = maxSwingX >= 0.0f || maxSwingY >= 0.0f;
      bool limitTwist = minTwist <= maxTwist;
      bool applyFriction = maxAngularImpulse > 0.0f;

      for(int i = 0; i < 2; ++i) {
        mEnforceDir[i] = LocalConstraint::NoEnforce;
        mLambdaSum[i] = mBias[i] = 0.0f;
        //Max float assuming we'll enforce on each axis. Doesn't matter if NoEnforce and will be adjusted if applying friction
        mMaxSum[i] = std::numeric_limits<float>::max();
      }
      mLambdaSum[2] = mMaxSum[2] = 0.0f;

      if(!limitSwing && !limitTwist && !applyFriction)
        return;

      //Compute error in b's reference frame like Bullet
      //This is more consistent than world space, not entirely sure why
      Quat rotRefA = rotA * refA;
      Quat rotRefB = rotB * refB;
      //Swing axis is forward for the reference bases
      Vec3 localSwing = Vec3::UnitZ;
      Quat atob = rotRefB.Inversed() * rotRefA;
      Vec3 swingInB = atob * localSwing;
      Quat bSwingFrame = Quat::GetRotation(localSwing, swingInB).Normalized();

      float halfSlop = LocalSphericalConstraint::sAngularSlop*0.5f;
      float swingError, swingAngle;
      Vec3 swingAxisB;
      ComputeSwingError(bSwingFrame, maxSwingX, maxSwingY, swingError, swingAxisB, swingAngle);

      bool fillSwingJac = applyFriction;
      if(swingAngle > LocalSphericalConstraint::sAngularSlop) {
        //As soon as there's any error, start enforcing, bias will only apply past half slop
        if(swingError > 0.0f) {
          //Only enforce positive direction to prevent further error
          mEnforceDir[0] = LocalConstraint::EnforcePos;
          mBias[0] = -Constraints::ComputeBiasPos(swingError, halfSlop, LocalConstraint::sVelBaumgarteTerm, LocalConstraint::sMaxVelCorrection);
          fillSwingJac = true;
        }
      }
      else {
        //No error, don't bother enforcing
        fillSwingJac = false;
      }

      //Either filling to prevent further swing error or to apply friction
      if(fillSwingJac) {
        //Angular limit will set to EnforcePos, so if we're here for friction, enforce in both directions
        if(mEnforceDir[0] == LocalConstraint::NoEnforce)
          mEnforceDir[0] = LocalConstraint::EnforceBoth;
        Vec3 swing = rotRefB*(-swingAxisB);
        Vec3 swingB = -swing;
        mAngular[0] = swing;
        mAngularMA[0] = inertiaA * swing;
        mAngularMB[0] = inertiaB * swingB;
        mConstraintMass[0] = SafeDivide(1.0f, swing.Dot(mAngularMA[0]) + swingB.Dot(mAngularMB[0]), SYX_EPSILON);
      }

      if(limitTwist || applyFriction) {
        Quat bTwistFrame = (bSwingFrame.Inversed() * atob).Normalized();
        float twistError;
        Vec3 twistAxisB;
        ComputeTwistError(bTwistFrame, twistError, twistAxisB);

        mAngular[1] = rotRefB*(-twistAxisB);
        if(limitTwist)
          Constraints::ComputeAngularLimitError(minTwist, maxTwist, applyFriction, twistError, mEnforceDir[1]);
        else
          mEnforceDir[1] = LocalConstraint::EnforceBoth;

        if(mEnforceDir[1] != LocalConstraint::NoEnforce) {
          if(limitTwist) {
            mBias[1] = -Constraints::ComputeBias(twistError, halfSlop, LocalConstraint::sVelBaumgarteTerm, LocalConstraint::sMaxVelCorrection);
          }
          mAngularMA[1] = inertiaA * mAngular[1];
          mAngularMB[1] = inertiaB * -mAngular[1];
          mConstraintMass[1] = SafeDivide(1.0f, mAngular[1].Dot(mAngularMA[1]) - mAngular[1].Dot(mAngularMB[1]), SYX_EPSILON);
        }
      }

      if(applyFriction) {
        mAngular[2] = mAngular[0].Cross(mAngular[1]);
        mAngularMA[2] = inertiaA * mAngular[2];
        mAngularMB[2] = inertiaB * -mAngular[2];
        mConstraintMass[2] = SafeDivide(1.0f, mAngular[2].Dot(mAngularMA[2]) - mAngular[2].Dot(mAngularMB[2]), SYX_EPSILON);
        for(int i = 0; i < 3; ++i)
          mMaxSum[i] = maxAngularImpulse*mConstraintMass[i];
      }
    }

    void SwingTwistBlock::ComputeSwingError(const Quat& swingFrame, float maxSwingX, float maxSwingY, float& swingError, Vec3& swingAxis, float& swingAngle) {
      swingAngle = swingFrame.GetAngle();
      if(swingAngle) {
        swingAxis = swingFrame.GetAxis().SafeNormalized();
        if(maxSwingX <= 0.0f || maxSwingY <= 0.0f) {
          swingError = 0.0f;
          return;
        }
        //Angular limit is based on an ellipse. Rotate axis 90 degrees about swing to get point on ellipse.
        //Then get the intersection in that direction with ellipse to get angular limit
        //Rotate 90 degrees about Z: local swing axis
        Vec2 ellipseDir(-swingAxis.y, swingAxis.x);
        Vec2 ellipseScale(maxSwingX, maxSwingY);
        float t = EllipseLineIntersect2d(ellipseDir, ellipseScale);
        //Intersection point on ellipse
        Vec2 intersect = ellipseDir*t;
        //Distance to intersection
        float swingLimit = intersect.Length();

        //Now adjust swing axis to correct along the normal of the ellipse instead of towards the center
        Vec2 normal;
        EllipsePointToNormal(ellipseDir, ellipseScale, normal);

        swingError = swingAngle - swingLimit;
        //If point is outside of ellipse, calculate error along normal, as that's the direction we're resolving in
        if(swingError > SYX_EPSILON) {
          //Normalize ellipseDir using sqrt from swing limit above, then get the actual point out of the ellipse
          Vec2 ellipsePoint = intersect*(swingAngle/swingLimit);
          t = EllipseLineIntersect2d(ellipsePoint, -normal, ellipseScale);
          //Error is now distance from ellipsePoint to intersect along normal
          swingError = (normal*t).Length();
        }
      }
    }

    void SwingTwistBlock::ComputeTwistError(const Quat& twistFrame, float& twistAngle, Vec3& twistAxis) {
      Quat minTwist = twistFrame;
      twistAngle = twistFrame.GetAngle();
      //Ensure we're using the shorter twist arc, and if we aren't flip it
      if(twistAngle > SYX_PI) {
        minTwist = -minTwist;
        twistAngle = minTwist.GetAngle();
      }
      twistAxis = minTwist.GetAxis().SafeNormalized();
    }

    void SwingTwistBlock::ApplyImpulse(int index, float lambda, ConstraintObjBlock& a, ConstraintObjBlock& b) {
      Constraints::ApplyAngularImpulse(lambda, mAngularMA[index], mAngularMB[index], a, b);
    }

    float SwingTwistBlock::Solve(ConstraintObjBlock& a, ConstraintObjBlock& b) {
      float result = 0.0f;
      for(int i = 0; i < 2; ++i) {
        if(mEnforceDir[i] != LocalConstraint::NoEnforce) {
          float jv = mAngular[i].Dot(a.mAngVel - b.mAngVel);
          float lambda = Constraints::ComputeLambda(jv, mBias[i], mConstraintMass[i]);
          float minSum, maxSum;
          Constraints::ComputeLambdaBounds(mMaxSum[i], mEnforceDir[i], minSum, maxSum);
          Constraints::ClampLambda(lambda, mLambdaSum[i], minSum, maxSum);
          ApplyImpulse(i, lambda, a, b);
          result += std::abs(lambda);
        }
      }
      //3rd friction axis is always both directions so it's determined by having a nonzero max impulse
      if(mMaxSum[2]) {
        float jv = mAngular[2].Dot(a.mAngVel - b.mAngVel);
        float lambda = Constraints::ComputeLambda(jv, mConstraintMass[2]);
        Constraints::ClampLambda(lambda, mLambdaSum[2], -mMaxSum[2], mMaxSum[2]);
        ApplyImpulse(2, lambda, a, b);
        result += std::abs(lambda);
      }
      return result;
    }

    float SwingTwistBlock::SSolve(SFloats& angVelA, SFloats& angVelB) {
      //No SIMD right now
      angVelA=angVelA;
      angVelB=angVelB;
      return 0.0f;
    }

    void LocalSphericalConstraint::FirstIteration() {
      Constraints::LoadObjects(mBlockObjA, mBlockObjB, *mA, *mB);

      SphericalConstraint& owner = static_cast<SphericalConstraint&>(*mOwner);
      const Transform& ta = mA->mOwner->GetTransform();
      const Transform& tb = mB->mOwner->GetTransform();
      Vec3 worldAnchorA = ta.ModelToWorld(owner.GetLocalAnchor(ConstraintObj::A));
      Vec3 worldAnchorB = tb.ModelToWorld(owner.GetLocalAnchor(ConstraintObj::B));
      testA = worldAnchorA;
      mLinearBlock.Setup(mBlockObjA.mPos, mBlockObjB.mPos, worldAnchorA, worldAnchorB, mA->mInvMass, mB->mInvMass, mA->mInertia, mB->mInertia);

      float maxSwingX, maxSwingY;
      owner.GetSwingLimits(maxSwingX, maxSwingY);
      float maxAngularImpulse = owner.GetMaxAngularImpulse();
      float minTwist, maxTwist;
      Quat rotRefA, rotRefB;
      owner.GetAngularReferences(rotRefA, rotRefB);
      owner.GetTwistLimits(minTwist, maxTwist);
      mAngularBlock.Setup(rotRefA, rotRefB, ta.mRot, tb.mRot, mA->mInertia, mB->mInertia, maxSwingX, maxSwingY, minTwist, maxTwist, maxAngularImpulse);

      //Warm start angular
      for(int i = 0; i < 2; ++i)
        if(mAngularBlock.mEnforceDir[i] != LocalConstraint::NoEnforce) {
          mAngularBlock.ApplyImpulse(i, owner.mAngularWarmStart[i], mBlockObjA, mBlockObjB);
          mAngularBlock.mLambdaSum[i] = owner.mAngularWarmStart[i];
        }
      if(mAngularBlock.mMaxSum[2]) {
        mAngularBlock.ApplyImpulse(2, owner.mAngularWarmStart[2], mBlockObjA, mBlockObjB);
        mAngularBlock.mLambdaSum[2] = owner.mAngularWarmStart[2];
      }

      //Warm start linear
      mLinearBlock.ApplyImpulse(owner.mLinearWarmStart, mBlockObjA, mBlockObjB);
      mBlockObjA.StoreVelocity(*mA);
      mBlockObjB.StoreVelocity(*mB);
    }

    void LocalSphericalConstraint::LastIteration() {
      SphericalConstraint& owner = static_cast<SphericalConstraint&>(*mOwner);
      owner.mLinearWarmStart = mLinearBlock.mLambdaSum;
      for(int i = 0; i < 3; ++i)
        owner.mAngularWarmStart[i] = mAngularBlock.mLambdaSum[i];
    }

    float LocalSphericalConstraint::Solve() {
      Constraints::LoadVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
      float result = mAngularBlock.Solve(mBlockObjA, mBlockObjB);
      result += mLinearBlock.Solve(mBlockObjA, mBlockObjB);
      Constraints::StoreVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
      return result;
    }

    float LocalSphericalConstraint::SSolve() {
      SFloats linVelA, angVelA, linVelB, angVelB;
      SConstraints::LoadVelocity(*mA, *mB, linVelA, angVelA, linVelB, angVelB);
      float result = mAngularBlock.SSolve(angVelA, angVelB);
      result += mLinearBlock.SSolve(linVelA, angVelA, linVelB, angVelB);
      SConstraints::StoreVelocity(linVelA, angVelA, linVelB, angVelB, *mA, *mB);
      return result;
    }

    void LocalSphericalConstraint::Draw() {
      Vec3 worldAnchorA = mA->mOwner->GetTransform().ModelToWorld(mOwner->GetLocalAnchor(ConstraintObj::A));
      Vec3 worldAnchorB = mB->mOwner->GetTransform().ModelToWorld(mOwner->GetLocalAnchor(ConstraintObj::B));
      DebugDrawer& d = DebugDrawer::Get();
      float s = 0.1f;
      d.SetColor(1.0f, 0.0f, 0.0f);
      DrawCube(worldAnchorA, s);
      d.SetColor(0.0f, 0.0f, 1.0f);
      d.DrawPoint(worldAnchorB, s);

      Quat rotA = mA->mRot;
      Quat rotB = mB->mRot;
      SphericalConstraint& owner = static_cast<SphericalConstraint&>(*mOwner);
      Quat refA = owner.mRefA;
      Quat refB = owner.mRefB;
      Quat rotRefA = rotA * refA;
      Quat rotRefB = rotB * refB;
      //Swing axis is forward for the reference bases
      Vec3 localSwing = Vec3::UnitZ;
      Quat atob = rotRefB.Inversed() * rotRefA;
      Vec3 swingInB = atob * localSwing;
      Quat bSwingFrame = Quat::GetRotation(localSwing, swingInB).Normalized();
      Vec2 ellipseScale(owner.mMaxSwingRadsX, owner.mMaxSwingRadsY);

      Vec3 swingAxisB = bSwingFrame.GetAxis().SafeNormalized();
      float es = 0.25f;
      d.SetColor(0.0f, 0.0f, 1.0f);
      d.DrawEllipse(worldAnchorB, rotRefB.GetRight()*ellipseScale.x*es, rotRefB.GetUp()*ellipseScale.y*es);
      float swingAngle = bSwingFrame.GetAngle();
      if(swingAngle) {
        Vec3 swingAxis = bSwingFrame.GetAxis();
        Vec2 ellipseDir(-swingAxis.y, swingAxis.x);
        Vec2 ellipsePoint = ellipseDir.Normalized()*swingAngle;
        Vec3 ep3 = Vec3(ellipsePoint.x, ellipsePoint.y, 0.0f);
        d.SetColor(1.0f, 1.0f, 1.0f);
        d.DrawVector(worldAnchorB, (rotRefB*ep3)*es);
      }

      Quat bTwistFrame = (bSwingFrame.Inversed() * atob).Normalized();
      float twistError;
      Vec3 twistAxisB;
      SwingTwistBlock::ComputeTwistError(bTwistFrame, twistError, twistAxisB);
      float ts = 0.1f;
      float ss = 0.05f;
      d.SetColor(0.0f, 1.0f, 0.0f);
      Vec3 arcStart = rotRefB.GetRight()*ts;
      Vec3 arcNormal = rotRefB*twistAxisB;
      d.DrawArc(worldAnchorB, arcStart, arcNormal, owner.mMaxTwistRads, ss);
      d.DrawArc(worldAnchorB, arcStart, arcNormal, owner.mMinTwistRads, ss);
      d.SetColor(1.0f, 1.0f, 1.0f);
      d.DrawArc(worldAnchorB, arcStart, arcNormal, twistError, ss);
    }
}