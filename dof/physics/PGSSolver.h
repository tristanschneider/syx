#pragma once

#include "glm/vec2.hpp"

namespace PGS {
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

    const float* getJacobianIndex(int i) const {
      return values + i*STRIDE;
    }
    float* getJacobianIndex(int i) {
      return values + i*STRIDE;
    }
    float* values{};
  };

  struct JacobianMapping {
    static constexpr int32_t STRIDE = 2;
    static constexpr uint8_t INFINITE_MASS = 255;
    std::pair<uint8_t, uint8_t> getPairForConstraint(int i) const {
      const uint8_t* base = bodyIndices + i*STRIDE;
      return { *base, *(base + 1) };
    }
    uint8_t* getPairPointerForConstraint(int i) {
      return bodyIndices + i*STRIDE;
    }

    uint8_t* bodyIndices{};
  };

  struct BodyVelocity {
    glm::vec2 linear{ 0 };
    float angular{};
  };

  struct ConstraintVelocity {
    //Linear x, y, and angular
    static constexpr int32_t STRIDE = 3;

    float* getObjectVelocity(uint8_t obj) {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }

    float* getObjectVelocity(uint8_t obj) const {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }

    BodyVelocity getBody(uint8_t obj) const {
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
    const float* getObjectMass(uint8_t obj) const {
      return values + static_cast<int32_t>(obj)*STRIDE;
    }
    float* getObjectMass(uint8_t obj) {
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

    uint8_t bodies{};
    uint8_t constraints{};
    uint8_t maxIterations{};
    float maxLambda{};
  };

  struct SolverStorage {
    static constexpr float UNLIMITED_MIN = std::numeric_limits<float>::lowest();
    static constexpr float UNLIMITED_MAX = std::numeric_limits<float>::max();

    void resize(uint8_t bodies, uint8_t constraints);
    uint8_t bodyCount() const;
    uint8_t constraintCount() const;
    SolveContext createContext();
    void setUniformLambdaBounds(float min, float max);
    void setVelocity(uint8_t body, const glm::vec2& linear, float angular);
    void setUniformMass(float inverseMass, float inverseInertia);
    void setMass(uint8_t body, float inverseMass, float inverseInertia);
    void setJacobian(uint8_t constraintIndex,
      uint8_t bodyA,
      uint8_t bodyB,
      const glm::vec2& linearA,
      float angularA,
      const glm::vec2& linearB,
      float angularB
    );
    void premultiply();

    std::vector<float> lambda;
    std::vector<float> bias;
    std::vector<float> lambdaMin;
    std::vector<float> lambdaMax;
    std::vector<float> diagonal;
    std::vector<float> velocity;
    std::vector<float> mass;
    std::vector<float> jacobian;
    std::vector<float> jacobianTMass;
    std::vector<uint8_t> jacobianMapping;
    uint8_t maxIterations{ 5 };
    float maxLambda{ 0.001f };
  };

  void solvePGS(SolveContext& solver);
  void solvePGSWarmStart(SolveContext& solver);
}