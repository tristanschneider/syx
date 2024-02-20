#pragma once

#include "glm/vec2.hpp"

namespace PGS1D {
  using BodyIndex = uint32_t;
  using ConstraintIndex = uint32_t;

  //Same idea as PGSSolver but only solving on a single axis
  struct Jacobian {
    //linear velocity one dimensional
    static constexpr int32_t BLOCK_SIZE = 1;
    static constexpr int32_t STRIDE = BLOCK_SIZE * 2;

    const float* getJacobianIndex(ConstraintIndex i) const {
      return values + i*STRIDE;
    }
    float* getJacobianIndex(ConstraintIndex i) {
      return values + i*STRIDE;
    }
    float* values{};
  };

  struct JacobianMapping {
    static constexpr int32_t STRIDE = 2;
    static constexpr BodyIndex INFINITE_MASS = std::numeric_limits<BodyIndex>::max();
    std::pair<BodyIndex, BodyIndex> getPairForConstraint(int i) const {
      const BodyIndex* base = bodyIndices + i*STRIDE;
      return { *base, *(base + 1) };
    }
    BodyIndex* getPairPointerForConstraint(int i) {
      return bodyIndices + i*STRIDE;
    }
    const BodyIndex* getPairPointerForConstraint(int i) const {
      return bodyIndices + i*STRIDE;
    }

    BodyIndex* bodyIndices{};
  };

  struct BodyVelocity {
    float linear{};
  };

  struct ConstraintVelocity {
    static constexpr int32_t STRIDE = 1;

    float* getObjectVelocity(BodyIndex obj) {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }

    float* getObjectVelocity(BodyIndex obj) const {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }

    BodyVelocity getBody(BodyIndex obj) const {
      const float* b = getObjectVelocity(obj);
      return { *b };
    }

    float* values{};
  };

  //Conceptually a square matrix for each body in the system, but only has values on the diagonal
  struct MassMatrix {
    //Inverse mass
    static constexpr int32_t STRIDE = 1;
    const float* getObjectMass(BodyIndex obj) const {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }
    float* getObjectMass(BodyIndex obj) {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }

    float* values{};
  };

  struct SolveContext {
    //Array of number of constraints
    float* lambda{};
    float* bias{};
    const float* lambdaMin{};
    const float* lambdaMax{};
    //Array of number of constraints
    float* diagonal{};
    ConstraintVelocity velocity;
    const MassMatrix mass;
    const Jacobian jacobian;
    const Jacobian jacobianTMass;
    const JacobianMapping mapping;

    BodyIndex bodies{};
    ConstraintIndex constraints{};
    uint8_t currentIteration{};
    uint8_t maxIterations{};
    float maxLambda{};
  };

  struct SolverStorage {
    static constexpr float UNLIMITED_MIN = std::numeric_limits<float>::lowest();
    static constexpr float UNLIMITED_MAX = std::numeric_limits<float>::max();

    void clear();
    void resize(BodyIndex bodies, ConstraintIndex constraints);
    BodyIndex bodyCount() const;
    ConstraintIndex constraintCount() const;
    SolveContext createContext();
    void setUniformLambdaBounds(float min, float max);
    void setVelocity(BodyIndex body, float v);
    void setUniformMass(float inverseMass);
    void setMass(BodyIndex body, float inverseMass);
    void setJacobian(ConstraintIndex constraintIndex,
      BodyIndex bodyA,
      BodyIndex bodyB,
      float velocityA,
      float velocityB
    );
    void setBias(ConstraintIndex constraintIndex, float bias);
    void setLambdaBounds(ConstraintIndex constraintIndex, float min, float max);
    void setWarmStart(ConstraintIndex constraintIndex, float warmStart);
    void premultiply();
    //Jacobian times velocity of the two bodies, meaning the relative velocity along the axis

    std::vector<float> lambda;
    std::vector<float> bias;
    std::vector<float> lambdaMin;
    std::vector<float> lambdaMax;
    std::vector<float> diagonal;
    std::vector<float> velocity;
    std::vector<float> mass;
    std::vector<float> jacobian;
    std::vector<float> jacobianTMass;
    std::vector<BodyIndex> jacobianMapping;
    uint8_t maxIterations{ 5 };
    float maxLambda{ 0.001f };
  };

  struct SolveResult {
    float remainingError{};
    //If iteration or error boundary has been reached. This means solving shoudl stop
    //but does not necessarily means the desired stability has been reached
    bool isFinished{};
  };

  //Iterate until solved
  SolveResult solvePGS(SolveContext& solver);
  //Iterate once
  SolveResult advancePGS(SolveContext& solver);
  void warmStart(SolveContext& solver);
  //Warm start and iterate until solved
  SolveResult solvePGSWarmStart(SolveContext& solver);
  float computeJV(ConstraintIndex constraintIndex, SolveContext& solver);
}