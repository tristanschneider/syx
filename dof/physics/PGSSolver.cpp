#pragma once
#include "Precompile.h"
#include "PGSSolver.h"

namespace PGS {
  float dot(const float* a, const float* b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
  }

  void applyImpulse(float* va, float lambda, const float* jacobianTMass) {
    for(int i = 0; i < 3; ++i) {
      va[i] += lambda*jacobianTMass[i];
    }
  }

  void applyImpulse(float* va, float* vb, float lambda, const float* jacobianTMass) {
    applyImpulse(va, lambda, jacobianTMass);
    applyImpulse(vb, lambda, jacobianTMass + 3);
  }

  void SolverStorage::resize(BodyIndex bodies, ConstraintIndex constraints) {
    lambda.resize(constraints);
    lambdaMin.resize(constraints);
    lambdaMax.resize(constraints);
    diagonal.resize(constraints);
    jacobian.resize(static_cast<size_t>(constraints) * Jacobian::STRIDE);
    jacobianTMass.resize(static_cast<size_t>(constraints) * Jacobian::STRIDE);
    bias.resize(constraints);
    jacobianMapping.resize(static_cast<size_t>(constraints) * JacobianMapping::STRIDE);
    mass.resize(static_cast<size_t>(bodies) * MassMatrix::STRIDE);
    velocity.resize(static_cast<size_t>(bodies) * ConstraintVelocity::STRIDE);
  }

  BodyIndex SolverStorage::bodyCount() const {
    return static_cast<BodyIndex>(mass.size() / MassMatrix::STRIDE);
  }

  ConstraintIndex SolverStorage::constraintCount() const {
    return static_cast<ConstraintIndex>(lambda.size());
  }

  SolveContext SolverStorage::createContext() {
    return {
      lambda.data(),
      bias.data(),
      lambdaMin.data(),
      lambdaMax.data(),
      diagonal.data(),
      ConstraintVelocity{ velocity.data() },
      MassMatrix{ mass.data() },
      Jacobian{ jacobian.data() },
      Jacobian{ jacobianTMass.data() },
      JacobianMapping{ jacobianMapping.data() },
      bodyCount(),
      constraintCount(),
      uint8_t{},
      maxIterations,
      maxLambda
    };
  }

  void SolverStorage::setUniformLambdaBounds(float min, float max) {
    for(float& l : lambdaMin) {
      l = min;
    }
    for(float& l : lambdaMax) {
      l = max;
    }
  }

  void SolverStorage::setBias(ConstraintIndex constraintIndex, float b) {
    bias[constraintIndex] = b;
  }

  void SolverStorage::setLambdaBounds(ConstraintIndex constraintIndex, float min, float max) {
    lambdaMin[constraintIndex] = min;
    lambdaMax[constraintIndex] = max;
  }

  void SolverStorage::setWarmStart(ConstraintIndex constraintIndex, float warmStart) {
    lambda[constraintIndex] = warmStart;
  }

  void SolverStorage::setVelocity(BodyIndex body, const glm::vec2& linear, float angular) {
    float* base = ConstraintVelocity{ velocity.data() }.getObjectVelocity(body);
    base[0] = linear.x;
    base[1] = linear.y;
    base[2] = angular;
  }

  void SolverStorage::setUniformMass(float inverseMass, float inverseInertia) {
    for(size_t i = 0; i + 1 < mass.size(); i += 2) {
      mass[i] = inverseMass;
      mass[i + 1] = inverseInertia;
    }
  }

  void SolverStorage::setMass(BodyIndex body, float inverseMass, float inverseInertia) {
    float* base = MassMatrix{ mass.data() }.getObjectMass(body);
    base[0] = inverseMass;
    base[1] = inverseInertia;
  }

  void SolverStorage::setJacobian(ConstraintIndex constraintIndex,
    BodyIndex bodyA,
    BodyIndex bodyB,
    const glm::vec2& linearA,
    float angularA,
    const glm::vec2& linearB,
    float angularB
  ) {
    BodyIndex* pair = JacobianMapping{ jacobianMapping.data() }.getPairPointerForConstraint(constraintIndex);
    pair[0] = bodyA;
    pair[1] = bodyB;
    float* j = Jacobian{ jacobian.data() }.getJacobianIndex(constraintIndex);
    j[0] = linearA.x;
    j[1] = linearA.y;
    j[2] = angularA;
    j[3] = linearB.x;
    j[4] = linearB.y;
    j[5] = angularB;
  }

  void SolverStorage::premultiply() {
    //Since mass is only used here, another option would be to not store it at all and have the caller provide it when setting the jacobian
    const Jacobian j{ jacobian.data() };
    Jacobian jm{ jacobianTMass.data() };
    const MassMatrix m{ mass.data() };
    const JacobianMapping mapping{ jacobianMapping.data() };
    for(uint8_t i = 0; i < constraintCount(); ++i) {
      auto [a, b] = mapping.getPairForConstraint(i);
      const float* jp = j.getJacobianIndex(i);
      float* jmp = jm.getJacobianIndex(i);
      const float* ma = m.getObjectMass(a);
      const float* mb = m.getObjectMass(b);
      jmp[0] = jp[0]*ma[0];
      jmp[1] = jp[1]*ma[0];
      jmp[2] = jp[2]*ma[1];

      jmp[3] = jp[3]*mb[0];
      jmp[4] = jp[4]*mb[0];
      jmp[5] = jp[5]*mb[1];
    }
  }

  SolveResult solvePGS(SolveContext& solver) {
    SolveResult result;
    do {
      result = advancePGS(solver);
    }
    while(!result.isFinished);
    return result;
  }

  SolveResult advancePGS(SolveContext& solver) {
    //Precompute the diagonal of the matrix on the first iteration
    //TODO: should premultiply step go here too?
    if(!solver.currentIteration) {
      for(uint8_t i = 0; i < solver.constraints; ++i) {
        const float* jma = solver.jacobianTMass.getJacobianIndex(i);
        const float* jmb = jma + Jacobian::BLOCK_SIZE;
        const float* ja = solver.jacobian.getJacobianIndex(i);
        const float* jb = ja + Jacobian::BLOCK_SIZE;
        solver.diagonal[i] = 1.0f / (dot(ja, jma) + dot(jb, jmb));
      }
    }

    SolveResult result;
    for(ConstraintIndex i = 0; i < solver.constraints; ++i) {
      auto [a, b] = solver.mapping.getPairForConstraint(i);
      const float* ja = solver.jacobian.getJacobianIndex(i);
      const float* jb = ja + Jacobian::BLOCK_SIZE;
      //TODO: infinite mass
      float* va = solver.velocity.getObjectVelocity(a);
      float* vb = solver.velocity.getObjectVelocity(b);
      //Normally this is division but it is inverted upon computing the diagonal above.
      float lambda = (solver.bias[i] - dot(ja, va) - dot(jb, vb))*solver.diagonal[i];
      const float prevLambda = solver.lambda[i];
      solver.lambda[i] = std::max(solver.lambdaMin[i], std::min(prevLambda + lambda, solver.lambdaMax[i]));
      lambda = solver.lambda[i] - prevLambda;

      applyImpulse(va, vb, lambda, solver.jacobianTMass.getJacobianIndex(i));
      result.remainingError = std::max(result.remainingError, std::abs(lambda));
    }

    ++solver.currentIteration;
    //Done if iteration cap has been reached or solution has reached desired stability
    result.isFinished = solver.currentIteration >= solver.maxIterations || result.remainingError <= solver.maxLambda;
    return result;
  }

  void warmStart(SolveContext& solver) {
    for(ConstraintIndex i = 0; i < solver.constraints; ++i) {
      auto [a, b] = solver.mapping.getPairForConstraint(i);
      //Convention is that infinite mass objects are a, and b would never also be infinite mass
      const float* ja = solver.jacobian.getJacobianIndex(i);
      const float* jb = ja + Jacobian::BLOCK_SIZE;
      if (a != JacobianMapping::INFINITE_MASS) {
        const float* ma = solver.mass.getObjectMass(a);
        float* va = solver.velocity.getObjectVelocity(a);
        //Linear
        va[0] += *ma*ja[0]*solver.lambda[i];
        va[1] += *ma*ja[1]*solver.lambda[i];
        //Angular
        va[2] += *(ma + 1)*ja[2]*solver.lambda[i];
      }

      const float* mb = solver.mass.getObjectMass(b);
      float* vb = solver.velocity.getObjectVelocity(b);
      //Linear
      vb[0] += *mb*jb[0]*solver.lambda[i];
      vb[1] += *mb*jb[1]*solver.lambda[i];
      //Angular
      vb[2] += *(mb + 1)*jb[2]*solver.lambda[i];
    }
  }

  SolveResult solvePGSWarmStart(SolveContext& solver) {
    warmStart(solver);
    return solvePGS(solver);
  }
}