#pragma once

#include "RuntimeDatabase.h"
#include "StableElementID.h"
#include "glm/vec2.hpp"
#include "generics/IndexRange.h"

class RuntimeDatabaseTaskBuilder;

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

  struct ExternalTargetRow : Row<ExternalTarget> {};
  struct ConstraintSideRow : Row<ConstraintSide> {};
  struct ConstraintCommonRow : Row<ContraintCommon> {};

  //A constraint definition must have two targets, of which one can be "NoTarget"
  using ExternalTargetRowAlias = QueryAlias<ExternalTargetRow>;
  using ConstraintSideRowAlias = QueryAlias<ConstraintSideRow>;
  using ConstraintCommonRowAlias = QueryAlias<ConstraintCommonRow>;

  struct Rows;

  struct Definition {
    using Target = std::variant<NoTarget, ExternalTargetRowAlias, SelfTarget>;

    Rows resolve(RuntimeDatabaseTaskBuilder& task, const TableID& table);

    Target targetA, targetB;
    //A side may be empty if the target is NoTarget
    ConstraintSideRowAlias sideA, sideB;
    ConstraintCommonRowAlias common;
  };

  //Resolved version of all of the aliases in definition
  struct Rows {
    using Target = std::variant<NoTarget, ExternalTargetRow*, SelfTarget>;
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

  //When constraints are created an SP::IStorageModifier must be used to notify the object graph that they exist and upon removal
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
}