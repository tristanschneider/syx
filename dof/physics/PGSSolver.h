#pragma once

#include "glm/vec2.hpp"

namespace PGS {
  using BodyIndex = uint32_t;
  using ConstraintIndex = uint32_t;

  //Constraint between ab(1) and bc(2) and ac(3)
  //V = [va]
  //    [wa]
  //    [vb]
  //    [wb]
  //    [vc]
  //    [wc]

  //M = [ma]
  //    [ia]
  //    [mb]
  //    [ib]
  //    [mc]
  //    [ic]

  //Conceptually the Jacobian looks like this
  //J = [Ja1, Jb1,   0]
  //    [  0, Jb2, Jc2]
  //    [Ja3,   0, Jc3]
  //But every constraint is pairwise meaning a row can't have more than two objects in it
  //Those zeroes can be flattened and then a map used to index into it
  //Solver iterates over rows directly to get the jacobian pair, then uses JMap to get the velocity vector
  //J = [Ja1, Jb1]
  //    [Jb2, Jc2]
  //    [Ja3, Jc3]

  //Lookups are [constraint][obj] so constraint index 0 is between objects a and b
  //Zero can be used as a special case index for infinite mass objects
  //JMap = [a, b]
  //       [b, c]
  //       [a, c]

  //Solve Ax+b where A = J*(M^-1)*(Jt)
  //x is the constraint force to solve for
  //b is the desired bias

  //Conceptually this is s by 3n for s constraints and n bodies where each row is a constraint and each column is a body
  //Since the constraints are all pairwise most entries are empty. Instead of building the giant matrix
  //blocks of 6 are inserted wherever and the JacobianMapping is used to determine what's what
  //All jacobians can be inserted in whatever order
  //Rows are constraints, columns are bodies
  struct Jacobian {
    //linear velocity x, y, and angular velocity
    static constexpr int32_t BLOCK_SIZE = 3;
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
    glm::vec2 linear{ 0 };
    float angular{};
  };

  struct ConstraintVelocity {
    //Linear x, y, and angular
    static constexpr int32_t STRIDE = 3;

    float* getObjectVelocity(BodyIndex obj) {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }

    float* getObjectVelocity(BodyIndex obj) const {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }

    BodyVelocity getBody(BodyIndex obj) const {
      const float* b = getObjectVelocity(obj);
      return {
        glm::vec2{ b[0], b[1] },
        b[2]
      };
    }

    float* values{};
  };

  //Conceptually a square matrix for each body in the system, but only has values on the diagonal
  struct MassMatrix {
    //Pairs of inverse mass and inverse inertia
    static constexpr int32_t STRIDE = 2;
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

  struct BodyStorage {
    void resize(BodyIndex bodies);
    BodyIndex size() const;

    void setVelocity(BodyIndex body, const glm::vec2& linear, float angular);
    void setUniformMass(float inverseMass, float inverseInertia);
    void setMass(BodyIndex body, float inverseMass, float inverseInertia);

    std::vector<float> velocity;
    std::vector<float> mass;
  };

  struct ConstraintStorage {
    void resize(ConstraintIndex constraints);
    ConstraintIndex size() const;

    void setUniformLambdaBounds(float min, float max);
    void setJacobian(ConstraintIndex constraintIndex,
      BodyIndex bodyA,
      BodyIndex bodyB,
      const glm::vec2& linearA,
      float angularA,
      const glm::vec2& linearB,
      float angularB
    );
    void setBias(ConstraintIndex constraintIndex, float bias);
    void setLambdaBounds(ConstraintIndex constraintIndex, float min, float max);
    void setWarmStart(ConstraintIndex constraintIndex, float warmStart);

    std::vector<float> lambda;
    std::vector<float> bias;
    std::vector<float> lambdaMin;
    std::vector<float> lambdaMax;
    std::vector<float> diagonal;
    std::vector<float> jacobian;
    std::vector<float> jacobianTMass;
    std::vector<BodyIndex> jacobianMapping;
  };

  struct SolverConfig {
    uint8_t maxIterations{ 5 };
    float maxLambda{ 0.001f };
  };

  void premultiply(ConstraintStorage& constraints, const BodyStorage& bodies, ConstraintIndex begin, ConstraintIndex end);
  SolveContext createSolveContext(const SolverConfig& config, ConstraintStorage& constraints, BodyStorage& bodies);

  struct SolverStorage {
    static constexpr float UNLIMITED_MIN = std::numeric_limits<float>::lowest();
    static constexpr float UNLIMITED_MAX = std::numeric_limits<float>::max();

    //TODO: update call sites to use BodyStorage and ConstraintStorage directly?
    void clear();
    void resizeBodies(BodyIndex bodies);
    void resizeConstraints(ConstraintIndex constraints);
    void resize(BodyIndex bodies, ConstraintIndex constraints);
    BodyIndex bodyCount() const;
    ConstraintIndex constraintCount() const;
    SolveContext createContext();
    void setUniformLambdaBounds(float min, float max);
    void setVelocity(BodyIndex body, const glm::vec2& linear, float angular);
    void setUniformMass(float inverseMass, float inverseInertia);
    void setMass(BodyIndex body, float inverseMass, float inverseInertia);
    void setJacobian(ConstraintIndex constraintIndex,
      BodyIndex bodyA,
      BodyIndex bodyB,
      const glm::vec2& linearA,
      float angularA,
      const glm::vec2& linearB,
      float angularB
    );
    void setBias(ConstraintIndex constraintIndex, float bias);
    void setLambdaBounds(ConstraintIndex constraintIndex, float min, float max);
    void setWarmStart(ConstraintIndex constraintIndex, float warmStart);
    void premultiply();

    BodyStorage bodies;
    ConstraintStorage constraints;
    SolverConfig config;
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
  SolveResult advancePGS(SolveContext& solver, size_t begin, size_t end);
  void advanceIteration(SolveContext& solver, SolveResult& result);
  void warmStart(SolveContext& solver);
  void warmStart(SolveContext& solver, ConstraintIndex begin, ConstraintIndex end);
  //Warm start and iterate until solved
  SolveResult solvePGSWarmStart(SolveContext& solver);
  float computeJV(ConstraintIndex constraintIndex, SolveContext& solver);
}