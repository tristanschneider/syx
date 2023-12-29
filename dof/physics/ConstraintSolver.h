#pragma once

#include "Geometric.h"
#include "Table.h"

class IAppBuilder;
struct PhysicsAliases;

namespace ConstraintSolver {
  struct SolverGlobals {
    constexpr static float BIAS_DEFAULT = 0.03f;
    constexpr static float SLOP_DEFAULT = 0.005f;

    const float* biasTerm{};
    const float* slop{};
  };
  struct Material {
    //Proportion of the normal force that is used to negate motion orthogonal to the normal
    float frictionCoefficient{ 0.8f };
    //Proportion of the normal force that is set as the target separating velocity
    float restitutionCoefficient{ 0.0f };
  };

  //If both objects in the spatial pairs storage have this and the bitwise and is nonzero the constraint is solved
  using ConstraintMask = uint8_t;
  static constexpr ConstraintMask MASK_SOLVE_NONE{};
  static constexpr ConstraintMask MASK_SOLVE_ALL{ static_cast<ConstraintMask>(~MASK_SOLVE_NONE) };

  using BodyMass = Geo::BodyMass;

  struct SharedMaterialRow : SharedRow<Material> {};
  struct SharedMassRow : SharedRow<BodyMass> {};
  struct MassRow : Row<BodyMass> {};
  struct ConstraintMaskRow : Row<ConstraintMask> {};

  //Uses the islands from SpatialQueryStorage and the narrowphase information stored there from the narrowphase
  //to solve the constraints. The velocities for it are extracted from the corresponding physics aliases then written back out
  void solveConstraints(IAppBuilder& builder, const PhysicsAliases& tables, const SolverGlobals& globals);
}