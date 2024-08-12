#pragma once

#include "Constraints.h"
#include "StableElementID.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include "IslandGraph.h"

class IAppBuilder;
struct DBEvents;
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
    void clear() {
      info.reset();
    }

    bool isTouching() const;

    //Populated if contact constraint solving on the Z axis is desired
    std::optional<ZInfo> info;
  };
  struct ConstraintManifold {
    Constraints::ConstraintSide sideA;
    Constraints::ConstraintSide sideB;
    Constraints::ContraintCommon common;
  };
  enum class PairType : uint8_t {
    ContactXY,
    ContactZ,
    Constraint
  };

  //Points at the source of truth for the object's transform and velocity
  struct ObjA : Row<ElementRef> {};
  struct ObjB : Row<ElementRef> {};
  struct ManifoldRow : Row<ContactManifold> {};
  struct ZManifoldRow : Row<ZContactManifold> {};
  struct ConstraintRow : Row<ConstraintManifold> {};
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
    ZManifoldRow
  >;

  size_t addIslandEdge(ITableModifier& modifier,
    IslandGraph::Graph& graph,
    ObjA& rowA,
    ObjB& rowB,
    const ElementRef& a,
    const ElementRef& b
  );

  //Take the pair gains/losses from the broadphase and use them to create or remove entries in the SpatialPairsTable and their edges in the IslandGraph
  void updateSpatialPairsFromBroadphase(IAppBuilder& builder);

  struct IStorageModifier {
    virtual ~IStorageModifier() = default;
    virtual void addSpatialNode(const ElementRef& node, bool isImmobile) = 0;
    virtual void removeSpatialNode(const ElementRef& node) = 0;
    virtual void changeMobility(const ElementRef& node, bool isImmobile) = 0;
  };
  //Broadphase is responsible for informing this of new nodes to track or remove
  std::shared_ptr<IStorageModifier> createStorageModifier(RuntimeDatabaseTaskBuilder& task);
};