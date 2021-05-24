#include "Precompile.h"
#include "SyxSphericalConstraint.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"
#include "SyxPhysicsObject.h"

namespace Syx {
    const float LocalSphericalConstraint::sLinearSlop = 0.05f;
    const float LocalSphericalConstraint::sAngularSlop = 0.05f;

    //Cross vector with cardinal axis indicated by index, 0=x,1=y,2=z
    static Vec3 crossCardinal(const Vec3& v, unsigned int axis) {
      switch(axis) {
        case 0: return Vec3(0.0f, v.z, -v.y);
        case 1: return Vec3(-v.z, 0.0f, v.x);
        case 2: return Vec3(v.y, -v.x, 0.0f);
        default: SyxAssertError(false, "Invalid index"); return Vec3();
      }
    }

    void SphericalConstraint::setSwingFrame(const Quat& aFrame) {
      mRefA = aFrame;
      //Transform reference A to world then into local b
      mRefB = getObjB()->getTransform().mRot.inversed() * getObjA()->getTransform().mRot * mRefA;
    }

    void SphericalConstraint::getAngularReferences(Quat& ra, Quat& rb) const {
      ra = mRefA;
      rb = mRefB;
    }

    void SphericalConstraint::getSwingLimits(float& maxRadsX, float& maxRadsY) const {
      maxRadsX = mMaxSwingRadsX;
      maxRadsY = mMaxSwingRadsY;
    }

    void SphericalConstraint::setSwingLimits(float maxRadsX, float maxRadsY) {
      mMaxSwingRadsX = maxRadsX;
      mMaxSwingRadsY = maxRadsY;
    }

    void SphericalConstraint::getTwistLimits(float& minRads, float& maxRads) const {
      minRads = mMinTwistRads;
      maxRads = mMaxTwistRads;
    }

    void SphericalConstraint::setTwistLimits(float minRads, float maxRads) {
      mMinTwistRads = minRads;
      mMaxTwistRads = maxRads;
    }

    void SphericalConstraint::setMaxAngularImpulse(float max) {
      mMaxAngularImpulse = max;
    }

    float SphericalConstraint::getMaxAngularImpulse() const {
      return mMaxAngularImpulse;
    }

    void SphericalBlock::setup(const Vec3& posA, const Vec3& posB, const Vec3& anchorA, const Vec3& anchorB, float massA, float massB, const Mat3& inertiaA, const Mat3& inertiaB) {
      Vec3 ra = anchorA - posA;
      Vec3 rb = anchorB - posB;
      Vec3 error = anchorA - anchorB;
      float halfSlop = LocalSphericalConstraint::sLinearSlop*0.5f;

      for(int i = 0; i < 3; ++i) {
        mAngularA[i] = crossCardinal(ra, i);
        mAngularB[i] = -crossCardinal(rb, i);

        mAngularMA[i] = inertiaA*mAngularA[i];
        mAngularMB[i] = inertiaB*mAngularB[i];

        mBias[i] = Constraints::computeBias(error[i], halfSlop, LocalConstraint::sVelBaumgarteTerm, LocalConstraint::sMaxVelCorrection);
      }

      mMassA = massA;
      mMassB = massB;
      mLambdaSum = Vec3::Zero;

      //J*M^1*J^T with zero terms removed due to axes being orthogonal to each other
      const Vec3& rax = mAngularA[0]; const Vec3& ray = mAngularA[1]; const Vec3& raz = mAngularA[2];
      const Vec3& rbx = mAngularB[0]; const Vec3& rby = mAngularB[1]; const Vec3& rbz = mAngularB[2];
      const Vec3& rmax = mAngularMA[0]; const Vec3& rmay = mAngularMA[1]; const Vec3& rmaz = mAngularMA[2];
      const Vec3& rmbx = mAngularMB[0]; const Vec3& rmby = mAngularMB[1]; const Vec3& rmbz = mAngularMB[2];
      float mab = mMassA + mMassB;
      //Looks like it, but is not actually symmetric all the time
      mConstraintMass = Mat3(mab + rmax.dot(rax) + rmbx.dot(rbx), rmax.dot(ray) + rmbx.dot(rby), rmax.dot(raz) + rmbx.dot(rbz),
        rmay.dot(rax) + rmby.dot(rbx), mab + rmay.dot(ray) + rmby.dot(rby), rmay.dot(raz) + rmby.dot(rbz),
        rmaz.dot(rax) + rmbz.dot(rbx), rmaz.dot(ray) + rmbz.dot(rby), mab + rmaz.dot(raz) + rmbz.dot(rbz));
      mConstraintMass = mConstraintMass.inverse();
    }

    void SphericalBlock::applyImpulse(const Vec3& lambda, ConstraintObjBlock& a, ConstraintObjBlock& b) {
      for(int i = 0; i < 3; ++i) {
        Vec3 linearA, linearB;
        linearA = linearB = Vec3::Zero;
        linearA[i] = mMassA;
        linearB[i] = -mMassB;
        Constraints::applyImpulse(lambda[i], linearA, mAngularMA[i], linearB, mAngularMB[i], a, b);
      }
      mLambdaSum += lambda;
    }

    float SphericalBlock::solve(ConstraintObjBlock& a, ConstraintObjBlock& b) {
      Vec3 jv;
      for(int i = 0; i < 3; ++i) {
        //Dot product of cardinal axis with velocity of a and b, then the angular components as usual
        jv[i] = a.mLinVel[i] - b.mLinVel[i] + a.mAngVel.dot(mAngularA[i]) + b.mAngVel.dot(mAngularB[i]);
      }

      Vec3 lambda = Constraints::computeLambda(jv, mBias, mConstraintMass);
      applyImpulse(lambda, a, b);
      return std::abs(lambda.x) + std::abs(lambda.y) + std::abs(lambda.z);
    }

    float SphericalBlock::sSolve(SFloats& linVelA, SFloats& angVelA, SFloats& linVelB, SFloats& angVelB) {
      SFloats jv;
      SFloats velDiff = SSubAll(linVelA, linVelB);

      SFloats jvs[3];
      for(int i = 0; i < 3; ++i) {
        jvs[i] = SVec3::sum3(SAddAll(SMulAll(angVelA, SLoadAll(&mAngularA[i].x)), SMulAll(angVelB, SLoadAll(&mAngularB[i].x))));
        jvs[i] =  SAddAll(jvs[i], sSplatIndex(velDiff, i));
      }
      jv = sCombine(jvs[0], jvs[1], jvs[2]);

      SFloats lambda = SConstraints::computeLambda(jv, SLoadAll(&mBias.x), toSMat3(mConstraintMass));
      //Accumulate lambda sum
      Vec3 oldSum = mLambdaSum;
      SStoreAll(&mLambdaSum.x, SAddAll(SLoadAll(&mLambdaSum.x), lambda));

      //Splat mass into two vectors, we'll mask out non relevant components to form the linear portions of the jacobian
      SAlign Vec3 masses(mMassA, mMassA, mMassB, mMassB);
      SFloats massA = SLoadAll(&masses.x);
      SFloats massB = SVec3::neg(SShuffle(massA, 2, 2, 2, 2));
      massA = SShuffle(massA, 0, 0, 0, 0);
      for(int i = 0; i < 3; ++i) {
        SConstraints::applyImpulse(sSplatIndex(lambda, i), linVelA, angVelA, linVelB, angVelB,
          sMaskOtherIndices(massA, i), SLoadAll(&mAngularMA[i].x),
          sMaskOtherIndices(massB, i), SLoadAll(&mAngularMB[i].x));
      }

      lambda = SVec3::sum3(sAbsAll(lambda));
      SStoreAll(&masses.x, lambda);
      return masses.x;
    }

    void SwingTwistBlock::setup(const Quat& refA, const Quat& refB,
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
      Quat atob = rotRefB.inversed() * rotRefA;
      Vec3 swingInB = atob * localSwing;
      Quat bSwingFrame = Quat::getRotation(localSwing, swingInB).normalized();

      float halfSlop = LocalSphericalConstraint::sAngularSlop*0.5f;
      float swingError, swingAngle;
      Vec3 swingAxisB;
      computeSwingError(bSwingFrame, maxSwingX, maxSwingY, swingError, swingAxisB, swingAngle);

      bool fillSwingJac = applyFriction;
      if(swingAngle > LocalSphericalConstraint::sAngularSlop) {
        //As soon as there's any error, start enforcing, bias will only apply past half slop
        if(swingError > 0.0f) {
          //Only enforce positive direction to prevent further error
          mEnforceDir[0] = LocalConstraint::EnforcePos;
          mBias[0] = -Constraints::computeBiasPos(swingError, halfSlop, LocalConstraint::sVelBaumgarteTerm, LocalConstraint::sMaxVelCorrection);
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
        mConstraintMass[0] = safeDivide(1.0f, swing.dot(mAngularMA[0]) + swingB.dot(mAngularMB[0]), SYX_EPSILON);
      }

      if(limitTwist || applyFriction) {
        Quat bTwistFrame = (bSwingFrame.inversed() * atob).normalized();
        float twistError;
        Vec3 twistAxisB;
        computeTwistError(bTwistFrame, twistError, twistAxisB);

        mAngular[1] = rotRefB*(-twistAxisB);
        if(limitTwist)
          Constraints::computeAngularLimitError(minTwist, maxTwist, applyFriction, twistError, mEnforceDir[1]);
        else
          mEnforceDir[1] = LocalConstraint::EnforceBoth;

        if(mEnforceDir[1] != LocalConstraint::NoEnforce) {
          if(limitTwist) {
            mBias[1] = -Constraints::computeBias(twistError, halfSlop, LocalConstraint::sVelBaumgarteTerm, LocalConstraint::sMaxVelCorrection);
          }
          mAngularMA[1] = inertiaA * mAngular[1];
          mAngularMB[1] = inertiaB * -mAngular[1];
          mConstraintMass[1] = safeDivide(1.0f, mAngular[1].dot(mAngularMA[1]) - mAngular[1].dot(mAngularMB[1]), SYX_EPSILON);
        }
      }

      if(applyFriction) {
        mAngular[2] = mAngular[0].cross(mAngular[1]);
        mAngularMA[2] = inertiaA * mAngular[2];
        mAngularMB[2] = inertiaB * -mAngular[2];
        mConstraintMass[2] = safeDivide(1.0f, mAngular[2].dot(mAngularMA[2]) - mAngular[2].dot(mAngularMB[2]), SYX_EPSILON);
        for(int i = 0; i < 3; ++i)
          mMaxSum[i] = maxAngularImpulse*mConstraintMass[i];
      }
    }

    void SwingTwistBlock::computeSwingError(const Quat& swingFrame, float maxSwingX, float maxSwingY, float& swingError, Vec3& swingAxis, float& swingAngle) {
      swingAngle = swingFrame.getAngle();
      if(swingAngle) {
        swingAxis = swingFrame.getAxis().safeNormalized();
        if(maxSwingX <= 0.0f || maxSwingY <= 0.0f) {
          swingError = 0.0f;
          return;
        }
        //Angular limit is based on an ellipse. Rotate axis 90 degrees about swing to get point on ellipse.
        //Then get the intersection in that direction with ellipse to get angular limit
        //Rotate 90 degrees about Z: local swing axis
        Vec2 ellipseDir(-swingAxis.y, swingAxis.x);
        Vec2 ellipseScale(maxSwingX, maxSwingY);
        float t = ellipseLineIntersect2d(ellipseDir, ellipseScale);
        //Intersection point on ellipse
        Vec2 intersect = ellipseDir*t;
        //Distance to intersection
        float swingLimit = intersect.length();

        //Now adjust swing axis to correct along the normal of the ellipse instead of towards the center
        Vec2 normal;
        ellipsePointToNormal(ellipseDir, ellipseScale, normal);

        swingError = swingAngle - swingLimit;
        //If point is outside of ellipse, calculate error along normal, as that's the direction we're resolving in
        if(swingError > SYX_EPSILON) {
          //Normalize ellipseDir using sqrt from swing limit above, then get the actual point out of the ellipse
          Vec2 ellipsePoint = intersect*(swingAngle/swingLimit);
          t = ellipseLineIntersect2d(ellipsePoint, -normal, ellipseScale);
          //Error is now distance from ellipsePoint to intersect along normal
          swingError = (normal*t).length();
        }
      }
    }

    void SwingTwistBlock::computeTwistError(const Quat& twistFrame, float& twistAngle, Vec3& twistAxis) {
      Quat minTwist = twistFrame;
      twistAngle = twistFrame.getAngle();
      //Ensure we're using the shorter twist arc, and if we aren't flip it
      if(twistAngle > SYX_PI) {
        minTwist = -minTwist;
        twistAngle = minTwist.getAngle();
      }
      twistAxis = minTwist.getAxis().safeNormalized();
    }

    void SwingTwistBlock::applyImpulse(int index, float lambda, ConstraintObjBlock& a, ConstraintObjBlock& b) {
      Constraints::applyAngularImpulse(lambda, mAngularMA[index], mAngularMB[index], a, b);
    }

    float SwingTwistBlock::solve(ConstraintObjBlock& a, ConstraintObjBlock& b) {
      float result = 0.0f;
      for(int i = 0; i < 2; ++i) {
        if(mEnforceDir[i] != LocalConstraint::NoEnforce) {
          float jv = mAngular[i].dot(a.mAngVel - b.mAngVel);
          float lambda = Constraints::computeLambda(jv, mBias[i], mConstraintMass[i]);
          float minSum, maxSum;
          Constraints::computeLambdaBounds(mMaxSum[i], mEnforceDir[i], minSum, maxSum);
          Constraints::clampLambda(lambda, mLambdaSum[i], minSum, maxSum);
          applyImpulse(i, lambda, a, b);
          result += std::abs(lambda);
        }
      }
      //3rd friction axis is always both directions so it's determined by having a nonzero max impulse
      if(mMaxSum[2]) {
        float jv = mAngular[2].dot(a.mAngVel - b.mAngVel);
        float lambda = Constraints::computeLambda(jv, mConstraintMass[2]);
        Constraints::clampLambda(lambda, mLambdaSum[2], -mMaxSum[2], mMaxSum[2]);
        applyImpulse(2, lambda, a, b);
        result += std::abs(lambda);
      }
      return result;
    }

    float SwingTwistBlock::sSolve(SFloats& angVelA, SFloats& angVelB) {
      //No SIMD right now
      angVelA=angVelA;
      angVelB=angVelB;
      return 0.0f;
    }

    void LocalSphericalConstraint::firstIteration() {
      Constraints::loadObjects(mBlockObjA, mBlockObjB, *mA, *mB);

      SphericalConstraint& owner = static_cast<SphericalConstraint&>(*mOwner);
      const Transform& ta = mA->mOwner->getTransform();
      const Transform& tb = mB->mOwner->getTransform();
      Vec3 worldAnchorA = ta.modelToWorld(owner.getLocalAnchor(ConstraintObj::A));
      Vec3 worldAnchorB = tb.modelToWorld(owner.getLocalAnchor(ConstraintObj::B));
      mLinearBlock.setup(mBlockObjA.mPos, mBlockObjB.mPos, worldAnchorA, worldAnchorB, mA->mInvMass, mB->mInvMass, mA->mInertia, mB->mInertia);

      float maxSwingX, maxSwingY;
      owner.getSwingLimits(maxSwingX, maxSwingY);
      float maxAngularImpulse = owner.getMaxAngularImpulse();
      float minTwist, maxTwist;
      Quat rotRefA, rotRefB;
      owner.getAngularReferences(rotRefA, rotRefB);
      owner.getTwistLimits(minTwist, maxTwist);
      mAngularBlock.setup(rotRefA, rotRefB, ta.mRot, tb.mRot, mA->mInertia, mB->mInertia, maxSwingX, maxSwingY, minTwist, maxTwist, maxAngularImpulse);

      //Warm start angular
      for(int i = 0; i < 2; ++i)
        if(mAngularBlock.mEnforceDir[i] != LocalConstraint::NoEnforce) {
          mAngularBlock.applyImpulse(i, owner.mAngularWarmStart[i], mBlockObjA, mBlockObjB);
          mAngularBlock.mLambdaSum[i] = owner.mAngularWarmStart[i];
        }
      if(mAngularBlock.mMaxSum[2]) {
        mAngularBlock.applyImpulse(2, owner.mAngularWarmStart[2], mBlockObjA, mBlockObjB);
        mAngularBlock.mLambdaSum[2] = owner.mAngularWarmStart[2];
      }

      //Warm start linear
      mLinearBlock.applyImpulse(owner.mLinearWarmStart, mBlockObjA, mBlockObjB);
      mBlockObjA.storeVelocity(*mA);
      mBlockObjB.storeVelocity(*mB);
    }

    void LocalSphericalConstraint::lastIteration() {
      SphericalConstraint& owner = static_cast<SphericalConstraint&>(*mOwner);
      owner.mLinearWarmStart = mLinearBlock.mLambdaSum;
      for(int i = 0; i < 3; ++i)
        owner.mAngularWarmStart[i] = mAngularBlock.mLambdaSum[i];
    }

    float LocalSphericalConstraint::solve() {
      Constraints::loadVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
      float result = mAngularBlock.solve(mBlockObjA, mBlockObjB);
      result += mLinearBlock.solve(mBlockObjA, mBlockObjB);
      Constraints::storeVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
      return result;
    }

    float LocalSphericalConstraint::sSolve() {
      SFloats linVelA, angVelA, linVelB, angVelB;
      SConstraints::loadVelocity(*mA, *mB, linVelA, angVelA, linVelB, angVelB);
      float result = mAngularBlock.sSolve(angVelA, angVelB);
      result += mLinearBlock.sSolve(linVelA, angVelA, linVelB, angVelB);
      SConstraints::storeVelocity(linVelA, angVelA, linVelB, angVelB, *mA, *mB);
      return result;
    }

    void LocalSphericalConstraint::draw() {
      Vec3 worldAnchorA = mA->mOwner->getTransform().modelToWorld(mOwner->getLocalAnchor(ConstraintObj::A));
      Vec3 worldAnchorB = mB->mOwner->getTransform().modelToWorld(mOwner->getLocalAnchor(ConstraintObj::B));
      DebugDrawer& d = DebugDrawer::get();
      float s = 0.1f;
      d.setColor(1.0f, 0.0f, 0.0f);
      drawCube(worldAnchorA, s);
      d.setColor(0.0f, 0.0f, 1.0f);
      d.drawPoint(worldAnchorB, s);

      Quat rotA = mA->mRot;
      Quat rotB = mB->mRot;
      SphericalConstraint& owner = static_cast<SphericalConstraint&>(*mOwner);
      Quat refA = owner.mRefA;
      Quat refB = owner.mRefB;
      Quat rotRefA = rotA * refA;
      Quat rotRefB = rotB * refB;
      //Swing axis is forward for the reference bases
      Vec3 localSwing = Vec3::UnitZ;
      Quat atob = rotRefB.inversed() * rotRefA;
      Vec3 swingInB = atob * localSwing;
      Quat bSwingFrame = Quat::getRotation(localSwing, swingInB).normalized();
      Vec2 ellipseScale(owner.mMaxSwingRadsX, owner.mMaxSwingRadsY);

      Vec3 swingAxisB = bSwingFrame.getAxis().safeNormalized();
      float es = 0.25f;
      d.setColor(0.0f, 0.0f, 1.0f);
      d.drawEllipse(worldAnchorB, rotRefB.getRight()*ellipseScale.x*es, rotRefB.getUp()*ellipseScale.y*es);
      float swingAngle = bSwingFrame.getAngle();
      if(swingAngle) {
        Vec3 swingAxis = bSwingFrame.getAxis();
        Vec2 ellipseDir(-swingAxis.y, swingAxis.x);
        Vec2 ellipsePoint = ellipseDir.normalized()*swingAngle;
        Vec3 ep3 = Vec3(ellipsePoint.x, ellipsePoint.y, 0.0f);
        d.setColor(1.0f, 1.0f, 1.0f);
        d.drawVector(worldAnchorB, (rotRefB*ep3)*es);
      }

      Quat bTwistFrame = (bSwingFrame.inversed() * atob).normalized();
      float twistError;
      Vec3 twistAxisB;
      SwingTwistBlock::computeTwistError(bTwistFrame, twistError, twistAxisB);
      float ts = 0.1f;
      float ss = 0.05f;
      d.setColor(0.0f, 1.0f, 0.0f);
      Vec3 arcStart = rotRefB.getRight()*ts;
      Vec3 arcNormal = rotRefB*twistAxisB;
      d.drawArc(worldAnchorB, arcStart, arcNormal, owner.mMaxTwistRads, ss);
      d.drawArc(worldAnchorB, arcStart, arcNormal, owner.mMinTwistRads, ss);
      d.setColor(1.0f, 1.0f, 1.0f);
      d.drawArc(worldAnchorB, arcStart, arcNormal, twistError, ss);
    }
}