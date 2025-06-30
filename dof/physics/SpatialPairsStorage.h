#pragma once

#include "Constraints.h"
#include "StableElementID.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include "IslandGraph.h"

class IAppBuilder;
class RuntimeDatabaseTaskBuilder;
class ITableModifier;

namespace IslandGraph {
  struct Graph;
};

//This stores all persistent information between pairs of objects that are nearby
//The entries are created and destroyed as output from the broadphase and tracked by the island graph
namespace SP {
  struct ContactPoint {
    //Towards A
    glm::vec2 normal{};
    glm::vec2 centerToContactA{};
    glm::vec2 centerToContactB{};
    float overlap{};
    float contactWarmStart{};
    float frictionWarmStart{};
  };
  struct ZInfo {
    float getOverlap() const;

    //Towards A
    float normal{};
    //Solving along Z sweeps to avoid collision in the first place so separation is the distance between the bodies
    //Can still be negative if something goes wrong in which case it's solved more like a typical collision
    float separation{};
  };
  struct ContactManifold {
    void clear() {
      size = 0;
    }
    const ContactPoint& operator[](uint32_t i) const {
      return points[i];
    }
    ContactPoint& operator[](uint32_t i) {
      return points[i];
    }

    std::array<ContactPoint, 2> points;
    uint32_t size{};
  };
  struct ZContactManifold {
    bool isTouching() const;
    void clear();
    bool isSet() const;

    //Valid if SP::PairTypeRow indicates ContactZ
    ZInfo info;
  };
  //Up to 3 constraints between the pair of objects as configured by Constraints.cpp
  //Since there are 3 degrees of freedom in 2D more should not be needed
  struct ConstraintManifold : Constraints::Constraint3DOF {
  };
  struct ZConstraintManifold : Constraints::ConstraintZ1DOF {
  };
  enum class PairType : uint8_t {
    ContactXY,
    ContactZ,
    Constraint,
    //Constraint on XY and Z
    ConstraintWithZ,
    //Constraint only on Z
    ConstraintZOnly
  };
  constexpr bool isContactPair(PairType t) {
    switch(t) {
    case PairType::ContactXY:
    case PairType::ContactZ:
      return true;
    default:
      return false;
    }
  }

  //Points at the source of truth for the object's transform and velocity
  struct ObjA : Row<ElementRef> {};
  struct ObjB : Row<ElementRef> {};
  struct ManifoldRow : Row<ContactManifold> {};
  struct ZManifoldRow : Row<ZContactManifold> {};
  struct ConstraintRow : Row<ConstraintManifold> {};
  struct ZConstraintRow : Row<ZConstraintManifold> {};
  struct IslandGraphRow : SharedRow<IslandGraph::Graph> {};
  struct PairTypeRow : Row<PairType> {};

  //For more direct lookups, all spatial pairs are stored in a single tale
  //The type of connection is determined by PairTypeRow which determines the mutually exclusive constraint types
  //The contact manifolds are populated by Narrowphase.cpp while the constraints are populated by Constraints.cpp
  //The entries themselves are created and destroyed through the IStorageModifier
  using SpatialPairsTable = Table<
    IslandGraphRow,
    ObjA,
    ObjB,
    PairTypeRow,
    ManifoldRow,
    ZManifoldRow,
    ConstraintRow,
    ZConstraintRow
  >;

  size_t addIslandEdge(ITableModifier& modifier,
    IslandGraph::Graph& graph,
    ObjA& rowA,
    ObjB& rowB,
    PairTypeRow& rowType,
    const ElementRef& a,
    const ElementRef& b,
    PairType type
  );

  //Take the pair gains/losses from the broadphase and use them to create or remove entries in the SpatialPairsTable and their edges in the IslandGraph
  void updateSpatialPairsFromBroadphase(IAppBuilder& builder);

  struct IStorageModifier {
    virtual ~IStorageModifier() = default;
    virtual void addSpatialNode(const ElementRef& node, bool isImmobile) = 0;
    virtual void removeSpatialNode(const ElementRef& node) = 0;
    virtual void changeMobility(const ElementRef& node, bool isImmobile) = 0;
    virtual size_t nodeCount() const = 0;
    virtual size_t edgeCount() const = 0;
  };
  //Broadphase is responsible for informing this of new nodes to track or remove
  std::shared_ptr<IStorageModifier> createStorageModifier(RuntimeDatabaseTaskBuilder& task);
};