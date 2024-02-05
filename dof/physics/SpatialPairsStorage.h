#pragma once

#include "StableElementID.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include "IslandGraph.h"

class IAppBuilder;
struct DBEvents;
class RuntimeDatabaseTaskBuilder;

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

    //Populated if contact constraint solving on the Z axis is desired
    std::optional<ZInfo> info;
  };
  //Points at the source of truth for the object's transform and velocity
  struct ObjA : Row<StableElementID> {};
  struct ObjB : Row<StableElementID> {};
  struct ManifoldRow : Row<ContactManifold> {};
  struct ZManifoldRow : Row<ZContactManifold> {};
  struct IslandGraphRow : SharedRow<IslandGraph::Graph> {};

  using SpatialPairsTable = Table<
    IslandGraphRow,
    StableIDRow,
    ObjA,
    ObjB,
    ManifoldRow,
    ZManifoldRow
  >;

  //Take the pair gains/losses from the broadphase and use them to create or remove entries in the SpatialPairsTable and their edges in the IslandGraph
  void updateSpatialPairsFromBroadphase(IAppBuilder& builder);

  struct IStorageModifier {
    virtual ~IStorageModifier() = default;
    virtual void addSpatialNode(const StableElementID& node, bool isImmobile) = 0;
    virtual void removeSpatialNode(const StableElementID& node) = 0;
    virtual void changeMobility(const StableElementID& node, bool isImmobile) = 0;
  };
  //Broadphase is responsible for informing this of new nodes to track or remove
  std::shared_ptr<IStorageModifier> createStorageModifier(RuntimeDatabaseTaskBuilder& task);
};