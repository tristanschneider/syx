#pragma once

#include "RuntimeDatabase.h"
#include "StableElementID.h"
#include "glm/vec2.hpp"
#include "generics/IndexRange.h"
#include "generics/Hash.h"
#include "generics/Enum.h"
#include <bitset>
#include "generics/RateLimiter.h"
#include <variant>

class IAppBuilder;
class RuntimeDatabaseTaskBuilder;
struct PhysicsAliases;

namespace Constraints {
  struct ConstraintPair {
    auto operator<=>(const ConstraintPair&) const = default;
    ElementRef a, b;
  };
}

namespace ConstraintSolver {
  struct SolverGlobals;
};

namespace std {
  template<>
  struct hash<Constraints::ConstraintPair> {
    size_t operator()(const Constraints::ConstraintPair& pair) const {
      return gnx::Hash::combine(pair.a, pair.b);
    }
  };
}

//This is the interface that is used to add constraints to tables
//The rows (or derived classes of the rows) are added to tables which are referred to in the
//TableConstraintDefinitionsRow. This is what the physics system uses to find the constraints
//it needs to solve in the database.
//Targets can either refer to the elemnt itself in the table or be a reference to another element elsewhere
//This allows any number of constraints to be either baked into a table or used temporarily through storing in a constraint table
//The game uses stat effects to allow tasks to create their own temporary constraints
namespace Constraints {
  struct ExternalTarget {
    ElementRef target;
  };
  struct SelfTarget {};
  struct NoTarget {};

  //The configuration of one "side" of a pairwise constraint
  struct ConstraintSide {
    glm::vec2 linear{};
    float angular{};
  };
  struct ConstraintCommon {
    float lambdaMin{};
    float lambdaMax{};
    float bias{};
    float warmStart{};
  };
  struct Constraint3DOF {
    bool shouldSolve(int i = 0) const {
      return common[i].lambdaMin < common[i].lambdaMax;
    }

    int size() const {
      int result = 0;
      while(shouldSolve(result) && result < 3) {
        ++result;
      }
      return result;
    }

    //Caller must either fill all constraints or set this on the first sequential constraint from 0 that is not populated
    //A single constraint would call setEnd(1)
    void setEnd(int i) {
      common[i].lambdaMin = common[i].lambdaMax = 0;
    }

    std::array<Constraints::ConstraintSide, 3> sideA;
    std::array<Constraints::ConstraintSide, 3> sideB;
    std::array<Constraints::ConstraintCommon, 3> common;
  };
  struct ConstraintZ1DOF {
    bool shouldSolve() const {
      return common.lambdaMin < common.lambdaMax;
    }

    void clear() {
      common.lambdaMin = common.lambdaMax = 0;
    }

    //If lambda limits are set this is solved. Doesn't need a ConstraintSide as Z is only allowed to solve directly along that axis
    //This means the jacobian is always +Z for A and -Z for B, acting on the center of mass
    Constraints::ConstraintCommon common;
  };
  struct ConstraintStorage {
    constexpr static uint32_t INVALID = std::numeric_limits<uint32_t>::max();
    constexpr static uint32_t PENDING = INVALID - 1;

    auto operator<=>(const ConstraintStorage&) const = default;

    bool isValid() const {
      return indexVersion.first != INVALID;
    }

    bool isPending() const {
      return indexVersion.first == PENDING;
    }

    void clear() {
      indexVersion.first = INVALID;
    }

    void setPending() {
      indexVersion.first = PENDING;
    }

    void assign(std::pair<uint32_t, uint8_t> index) {
      indexVersion = index;
    }

    uint32_t getIndex() const {
      return indexVersion.first;
    }

    const std::pair<uint32_t, uint8_t>& getHandle() const {
      return indexVersion;
    }

    //EdgeIndex and EdgeVersion pair from IslandGraph whose index also matches SpatialPairsStorage
    std::pair<uint32_t, uint8_t> indexVersion{ std::make_pair(std::numeric_limits<uint32_t>::max(), uint8_t(0)) };
  };
  //Constraint is the low level solving mechanism, joint is a higher level description of the constraints the caller wants
  struct CustomJoint {};
  //One-dimensional constraint acting along the vector between the two anchor points to pin them together
  //Less stable than the 2D version
  struct PinJoint1D {
    glm::vec2 localCenterToPinA{};
    glm::vec2 localCenterToPinB{};
    float distance{};
  };
  struct PinJoint2D {
    glm::vec2 localCenterToPinA{};
    glm::vec2 localCenterToPinB{};
    float distance{};
  };
  //Allow no relative motion between the two bodies. The pins should match at a location that
  //causes the desired distance between the objects and the target orientation is the one that makes the two pins exactly face each-other
  struct WeldJoint {
    glm::vec2 localCenterToPinA{};
    glm::vec2 localCenterToPinB{};
    float allowedRotationRad{};
  };
  //Tries to achieve the target linear and angular velocity through a linear and angular impulse about the center of mass
  struct MotorJoint {
    enum class Flags : uint8_t {
      //If true, target velocity is in world space, otherwise local
      WorldSpaceLinear,
      //If true, the constraint tries to rotate the body so its angle matches angularTarget
      //If false, the target is a local space target velocity
      AngularOrientationTarget,
      //If true the motor will pull in the opposite direction if over the target speed
      CanPull,
      Count,
    };
    //Assumed in local space unless world space flag is set in which case it's local to object A
    //This is the target velocity the constraint is trying to reach
    glm::vec2 linearTarget{};
    //Only used if IncludeZ flag is set
    float linearTargetZ{};
    float angularTarget{};
    //Max force that can be used to achieve the target velocity
    float linearForce{};
    float angularForce{};
    float zForce{};
    //Scalar to the amount of error to try to correct every frame. Determines the amount constraint may overshoot its target
    float biasScalar{ 1.f };
    std::bitset<gnx::enumCast(Flags::Count)> flags{};
  };
  //Tries to achieve the target velocity at the anchor point in the desired direction
  struct PinMotorJoint {
    glm::vec2 localCenterToPinA{};
    glm::vec2 targetVelocity{};
    //Force along the primary direction
    float force{};
    //Force to counteract motion orthogonal to the target. Higher values prevent overshooting the target rotation
    float orthogonalForce{};
  };
  struct JointVariant {
    using Variant = std::variant<
      CustomJoint,
      PinJoint1D,
      PinJoint2D,
      WeldJoint,
      MotorJoint,
      PinMotorJoint
    >;
    Variant data;
  };

  //TODO: does the row make sense or should it just be what was in the IConstraintStorageModifier::insert?
  struct ExternalTargetRow : Row<ExternalTarget> {};
  //Custom constraints are bigger than all other joints so they use optional external storage here
  //To use a custom join specify the custom variant and set the values in this row
  struct CustomConstraintRow : Row<Constraint3DOF> {};
  struct ConstraintStorageRow : Row<ConstraintStorage> {};
  struct JointRow : Row<JointVariant> {};

  //If present, elements in constraint definitions for this table will be automatically initialzed using itself as a one-sided target
  struct AutoManageJointTag : TagRow{};

  //A constraint definition must have two targets, of which one can be "NoTarget"
  using ExternalTargetRowAlias = QueryAlias<ExternalTargetRow>;
  using ConstExternalTargetRowAlias = QueryAlias<const ExternalTargetRow>;
  using CustomConstraintRowAlias = QueryAlias<CustomConstraintRow>;
  using ConstraintStorageRowAlias = QueryAlias<ConstraintStorageRow>;
  using JointRowAlias = QueryAlias<JointRow>;

  struct Rows;
  using ConstraintDefinitionKey = size_t;

  struct Definition {
    using Target = std::variant<NoTarget, ExternalTargetRowAlias, SelfTarget>;

    Rows resolve(RuntimeDatabaseTaskBuilder& task, const TableID& table) const;
    static Rows resolve(RuntimeDatabaseTaskBuilder& task, const TableID& table, ConstraintDefinitionKey key);

    Target targetA, targetB;
    //A side may be empty if the target is NoTarget
    CustomConstraintRowAlias custom;
    ConstraintStorageRowAlias storage;
    JointRowAlias joint;
  };

  //Resolved version of all of the aliases in definition
  struct Rows {
    using Target = std::variant<NoTarget, ExternalTargetRow*, SelfTarget>;
    using ConstTarget = std::variant<NoTarget, const ExternalTargetRow*, SelfTarget>;
    Target targetA, targetB;
    CustomConstraintRow* custom{};
    JointRow* joint{};
  };

  struct TableConstraintDefinitions {
    std::vector<Definition> definitions;
  };

  //A table with constraints must declare this along with having the rows specified in its definitions
  struct TableConstraintDefinitionsRow : SharedRow<TableConstraintDefinitions> {};

  //Corresponds to a particular Definition
  struct OwnedConstraint {
    ElementRef owner;
    //Refers to the constraint that was originally assigned. This can differ from what is now in ConstraintStorageRow
    //in the case that the constraint was recreated. In that case the edge will eventually be removed by GC
    ConstraintStorage storage;
  };
  struct PendingConstraint {
    ElementRef owner, a, b;
  };
  struct PendingDefinitionConstraints {
    std::vector<PendingConstraint> constraints;
  };
  struct OwnedDefinitionConstraints {
    using RL = decltype(gnx::make_rate_limiter<200>());
    RL gcRate;
    std::vector<OwnedConstraint> constraints;
  };

  //Gains populated by a caller creating or removing constraints via IConstraintStorageModifier
  //Losses populated by garbage collection
  //Cleared by SpatialPairsStorage
  //TODO: maybe simpler if it was all populated and cleared in AssignStorage/GC
  struct ConstraintChanges {
    std::vector<ConstraintPair> gained;
    std::vector<ConstraintStorage> lost;
    //Normal pair tracking happens through broadphase updates
    //Manually created constraint pairs are tracked here and evaluated with occasional
    //garbage collection sweeps to ensure the non-null elements exist

    //One entry for each definition
    //Constraints that have been assigned a storage entry in SpatialPairsStorage
    std::vector<OwnedDefinitionConstraints> trackedConstraints;
    //Constraints that are awaiting storage in SpatialPairsStorage. Once they get it they move to trackedConstraints
    std::vector<PendingDefinitionConstraints> pendingConstraints;
    std::unordered_set<uint32_t> ownedEdges;
  };
  struct ConstraintChangesRow : SharedRow<ConstraintChanges> {};

  //Used to create the backing storage for the solving of the constraint pair
  //Separate from Builder because it requires more exclusive access and only needs to
  //be done once for the lifetime of the constraint, while builder may be used to reconfigure a constraint every frame
  //This could also be done with events but reassigning targets would still require this step
  class IConstraintStorageModifier {
  public:
    virtual ~IConstraintStorageModifier() = default;
    //Must be done for a pair before constraint solving will take effect
    virtual void insert(size_t tableIndex, const ElementRef& a, const ElementRef& b) = 0;
    //Can be used to explicitly remove a pair. Optional in the case of either element being deleted as GC will eventually see it
    virtual void erase(size_t tableIndex, const ElementRef& a, const ElementRef& b) = 0;
  };
  std::shared_ptr<IConstraintStorageModifier> createConstraintStorageModifier(RuntimeDatabaseTaskBuilder& task, ConstraintDefinitionKey constraintKey, const TableID& constraintTable);

  //When constraints are created an IConstraintStorageModifier must be used to notify the object graph that they exist and upon removal
  //Stats do this in their update

  //Used by the game to configure any valid constraint rows
  class Builder {
  public:
    Builder(const Rows& r);

    Builder& select(const gnx::IndexRange& range);
    Builder& setJointType(const JointVariant& joint);
    Builder& setTargets(const ElementRef& a, const ElementRef& b);

  private:
    gnx::IndexRange selected;
    Rows rows;
  };

  //Init constraint tables from definitions. Should be called after all tables have had a chance to fill their definitions
  void init(IAppBuilder& builder);

  //The constraint equivalent of a narrowphase for collision detection
  //This populates the ConstraintManifold for all manually added constraints
  void constraintNarrowphase(IAppBuilder& builder, const PhysicsAliases& aliases, const ConstraintSolver::SolverGlobals& globals);
  void garbageCollect(IAppBuilder& builder);
  void assignStorage(IAppBuilder& builder);

  void update(IAppBuilder& builder, const PhysicsAliases& aliases, const ConstraintSolver::SolverGlobals& globals);

  void postProcessEvents(IAppBuilder& builder);
}
