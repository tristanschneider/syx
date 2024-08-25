#pragma once

#include "RuntimeDatabase.h"
#include "StableElementID.h"
#include "glm/vec2.hpp"
#include "generics/IndexRange.h"
#include "generics/Hash.h"

class IAppBuilder;
class RuntimeDatabaseTaskBuilder;

namespace Constraints {
  struct ConstraintPair {
    auto operator<=>(const ConstraintPair&) const = default;
    ElementRef a, b;
  };
}

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
  struct ContraintCommon {
    float lambdaMin{};
    float lambdaMax{};
    float bias{};
  };
  struct ConstraintStorage {
    constexpr static size_t INVALID = std::numeric_limits<size_t>::max();

    bool isValid() const {
      return storageIndex != INVALID;
    }

    //Entry for this constraint in SpatialPairsStorage
    size_t storageIndex{ std::numeric_limits<size_t>::max() };
  };

  struct ExternalTargetRow : Row<ExternalTarget> {};
  struct ConstraintSideRow : Row<ConstraintSide> {};
  struct ConstraintCommonRow : Row<ContraintCommon> {};
  struct ConstraintStorageRow : Row<ConstraintStorage> {};

  //A constraint definition must have two targets, of which one can be "NoTarget"
  using ExternalTargetRowAlias = QueryAlias<ExternalTargetRow>;
  using ConstExternalTargetRowAlias = QueryAlias<const ExternalTargetRow>;
  using ConstraintSideRowAlias = QueryAlias<ConstraintSideRow>;
  using ConstraintCommonRowAlias = QueryAlias<ConstraintCommonRow>;
  using ConstraintStorageRowAlias = QueryAlias<ConstraintStorageRow>;

  struct Rows;

  struct Definition {
    using Target = std::variant<NoTarget, ExternalTargetRowAlias, SelfTarget>;

    Rows resolve(RuntimeDatabaseTaskBuilder& task, const TableID& table);

    Target targetA, targetB;
    //A side may be empty if the target is NoTarget
    ConstraintSideRowAlias sideA, sideB;
    ConstraintCommonRowAlias common;
    ConstraintStorageRowAlias storage;
  };

  //Resolved version of all of the aliases in definition
  struct Rows {
    using Target = std::variant<NoTarget, ExternalTargetRow*, SelfTarget>;
    using ConstTarget = std::variant<NoTarget, const ExternalTargetRow*, SelfTarget>;
    Target targetA, targetB;
    //A side may be empty if the target is NoTarget
    ConstraintSideRow* sideA{};
    ConstraintSideRow* sideB{};
    ConstraintCommonRow* common{};
  };

  struct TableConstraintDefinitions {
    std::vector<Definition> definitions;
  };

  //A table with constraints must declare this along with having the rows specified in its definitions
  struct TableConstraintDefinitionsRow : SharedRow<TableConstraintDefinitions> {};

  using ConstraintDefinitionKey = size_t;

  //Populated by a caller creating or removing constraints via IConstraintStorageModifier
  struct ConstraintChanges {
    std::vector<ConstraintPair> gained, lost;
    //Normal pair tracking happens through broadphase updates
    //Manually created constraint pairs are tracked here and evaluated with occasional
    //garbage collection sweeps to ensure the non-null elements exist

    //TODO: this doesn't work for multiple constraints between two bodies
    std::unordered_set<ConstraintPair> trackedPairs;
    size_t ticksSinceGC{};
  };
  struct ConstraintChangesRow : SharedRow<ConstraintChanges> {};

  //Used to create the backing storage for the solving of the constraint pair
  //Separate from Builder because it requires more exclusive access and only needs to
  //be done once for the lifetime of the constraint, while builder may be used to reconfigure a constraint every frame
  //This could also be done with DBEvents but reassigning targets would still require this step
  class IConstraintStorageModifier {
  public:
    virtual ~IConstraintStorageModifier() = default;
    //Must be done for a pair before constraint solving will take effect
    virtual void insert(const ElementRef& a, const ElementRef& b) = 0;
    //Can be used to explicitly remove a pair. Optional in the case of either element being deleted as GC will eventually see it
    virtual void erase(const ElementRef& a, const ElementRef& b) = 0;
  };
  std::shared_ptr<IConstraintStorageModifier> createConstraintStorageModifier(RuntimeDatabaseTaskBuilder& task);

  //When constraints are created an IConstraintStorageModifier must be used to notify the object graph that they exist and upon removal
  //Stats do this in their update

  //Used by the game to configure any valid constraint rows
  class Builder {
  public:
    Builder(const Rows& r);

    Builder& select(const gnx::IndexRange& range);

  private:
    gnx::IndexRange selected;
    Rows rows;
  };

  //The constraint equivalent of a narrowphase for collision detection
  //This populates the ConstraintManifold for all manually added constraints
  void constraintNarrowphase(IAppBuilder& builder);
  void garbageCollect(IAppBuilder& builder);

  //TODO: call in simulation
}
