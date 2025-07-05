#pragma once

#include "Mass.h"
#include "Table.h"

class IAppBuilder;
struct PhysicsAliases;
class RuntimeDatabaseTaskBuilder;
struct UnpackedDatabaseElementID;

namespace ConstraintSolver {
  struct SolverGlobals {
    constexpr static float BIAS_DEFAULT = 0.03f;
    constexpr static float SLOP_DEFAULT = 0.005f;

    const float* biasTerm{};
    const float* slop{};
    //Not physically accurate but more stabile and cheaper
    //Uses a constant friction value rather than being a proportion of the normal force
    bool useConstantFriction = true;
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

  using BodyMass = Mass::OriginMass;

  struct BodyVelocity {
    operator bool() const {
      return linearX && linearY && angular;
    }
    float* linearX{};
    float* linearY{};
    float* angular{};
  };

  struct ConstraintBody {
    BodyVelocity velocity;
    ConstraintMask constraintMask{};
    std::optional<BodyMass> mass;
    const Material* material{};
  };

  struct IBodyResolver {
    virtual ~IBodyResolver() = default;
    virtual ConstraintBody resolve(const UnpackedDatabaseElementID& id) = 0;
  };

  struct SharedMaterialRow : SharedRow<Material> {};
  struct SharedMassRow : SharedRow<BodyMass> {};
  struct MassRow : Row<BodyMass> {};
  struct ConstraintMaskRow : Row<ConstraintMask> {
    static constexpr std::string_view KEY = "ConstraintMask";
  };

  //Uses the islands from SpatialQueryStorage and the narrowphase information stored there from the narrowphase
  //to solve the constraints. The velocities for it are extracted from the corresponding physics aliases then written back out
  void solveConstraints(IAppBuilder& builder, const PhysicsAliases& tables, const SolverGlobals& globals);

  std::unique_ptr<IBodyResolver> createResolver(RuntimeDatabaseTaskBuilder& task, const PhysicsAliases& tables);
}