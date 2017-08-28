#pragma once
#include "SyxConstraint.h"

namespace Syx {
  namespace SConstraints {
    FInline void storeVelocity(const SFloats& linVelA, const SFloats& angVelA, const SFloats& linVelB, const SFloats& angVelB, LocalObject& a, LocalObject&b) {
      SStoreAll(&a.mLinVel.x, linVelA);
      SStoreAll(&a.mAngVel.x, angVelA);
      SStoreAll(&b.mLinVel.x, linVelB);
      SStoreAll(&b.mAngVel.x, angVelB);
    }

    FInline void loadVelocity(const LocalObject& a, const LocalObject& b, SFloats& linVelA, SFloats& angVelA, SFloats& linVelB, SFloats& angVelB) {
      linVelA = SLoadAll(&a.mLinVel.x);
      angVelA = SLoadAll(&a.mAngVel.x);
      linVelB = SLoadAll(&b.mLinVel.x);
      angVelB = SLoadAll(&b.mAngVel.x);
    }

    FInline SFloats computeJV(const SFloats& jla, const SFloats& jaa, const SFloats& jlb, const SFloats& jab,
                              const SFloats& vla, const SFloats& vaa, const SFloats& vlb, const SFloats& vab) {
      return SVec3::sum3(SAddAll(SMulAll(jla, vla), SAddAll(SMulAll(jaa, vaa), SAddAll(SMulAll(jlb, vlb), SMulAll(jab, vab)))));
    }

    FInline SFloats computeLambda(SFloats jv, SFloats bias, SFloats constraintMass) {
      return SVec3::neg(SMulAll(SAddAll(jv, bias), constraintMass));
    }

    FInline SFloats computeLambda(SFloats jv, SFloats constraintMass) {
      return SVec3::neg(SMulAll(jv, constraintMass));
    }

    FInline SFloats computeLambda(SFloats jv, SFloats bias, const SMat3& constraintMass) {
      return constraintMass*SVec3::neg(SAddAll(jv, bias));
    }

    FInline void clampLambdaMin(SFloats& lambda, SFloats& lambdaSum, SFloats minBound) {
      SFloats oldSum = lambdaSum;
      lambdaSum = SAddAll(lambdaSum, lambda);
      lambdaSum = SMaxAll(lambdaSum, minBound);
      lambda = SSubAll(lambdaSum, oldSum);
    }

    FInline void clampLambdaMax(SFloats& lambda, SFloats& lambdaSum, SFloats maxBound) {
      SFloats oldSum = lambdaSum;
      lambdaSum = SAddAll(lambdaSum, lambda);
      lambdaSum = SMinAll(lambdaSum, maxBound);
      lambda = SSubAll(lambdaSum, oldSum);
    }

    FInline void clampLambda(SFloats& lambda, SFloats& lambdaSum, SFloats minBound, SFloats maxBound) {
      SFloats oldSum = lambdaSum;
      lambdaSum = SAddAll(lambdaSum, lambda);
      lambdaSum = SMaxAll(minBound, SMinAll(lambdaSum, maxBound));
      lambda = SSubAll(lambdaSum, oldSum);
    }

    FInline void applyImpulse(const SFloats& lambda, SFloats& vla, SFloats& vaa, SFloats& vlb, SFloats& vab,
                              const SFloats& jlam, const SFloats& jaam, const SFloats& jlbm, const SFloats& jabm) {
      vla = SAddAll(vla, SMulAll(lambda, jlam));
      vaa = SAddAll(vaa, SMulAll(lambda, jaam));
      vlb = SAddAll(vlb, SMulAll(lambda, jlbm));
      vab = SAddAll(vab, SMulAll(lambda, jabm));
    }
  }
}