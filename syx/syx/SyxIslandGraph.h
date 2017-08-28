#pragma once
#include "SyxStaticIndexable.h"

namespace Syx {
  class Constraint;
  class PhysicsObject;

  typedef size_t IslandIndex;
  typedef size_t IndexableKey;

  enum class SleepState : unsigned char {
    Active,
    Inactive,
    //Awake for the frame that state changed from inactive to active
    Awake,
    //Asleep for the frame that state changed from active to inactive
    Asleep,
  };

  //Wrap a small static container and a vector in one container as the
  //most common case won't need enough to touch the vector so we can avoid the cache misses and allocations
  struct IndexContainer {
  public:
    static const int sMaxStaticSize = 10;

    IndexContainer()
      : mSize(0) {
    }

    size_t size() const {
      return mSize;
    }

    void pushBack(IslandIndex obj);
    void remove(size_t index);
    IslandIndex operator[](size_t index) const;

    void clear();

  private:
    IslandIndex& _get(size_t index);

    IslandIndex mStatic[sMaxStaticSize];
    std::vector<IslandIndex> mDynamic;
    size_t mSize;
  };

  struct IslandNode {
    IslandNode();
    IslandNode(IslandIndex island)
      : mIsland(island) {
    }

    void removeEdge(IslandIndex edge);

    IndexableKey mIsland;
    IndexContainer mEdges;
  };

  struct IslandEdge {
    IslandEdge();
    IslandEdge(IslandIndex from, IslandIndex to, Constraint& constraint)
      : mFrom(from)
      , mTo(to)
      , mConstraint(&constraint) {}

    IslandIndex getOther(IslandIndex index) const {
      return index == mFrom ? mTo : mFrom;
    }

    static void link(IslandNode& a, IslandNode& b, IslandIndex edge);
    static void unlink(IslandNode& a, IslandNode& b, IslandIndex edge);

    Constraint* mConstraint;
    IslandIndex mFrom;
    IslandIndex mTo;
  };

  //Not sure what I'll want to be tacking on here, so wrapping it in a struct
  struct IslandContents {
    void clear() {
      mConstraints.clear();
    }

    std::vector<Constraint*> mConstraints;
    IndexableKey mIslandKey;
    SleepState mSleepState;
  };

  struct Island {
    //Seconds before island will fall asleep while inactive
    static float sTimeToSleep;

    Island();
    Island(IslandIndex root);

    void add(IndexableKey islandKey, IslandNode& node);
    void remove(IslandNode& node);
    void setInactive(float dt);
    void setActive(bool clearAwake = false);

    IslandIndex mRoot;
    size_t mSize;
    SleepState mSleepState;
    float mInactiveTime;
  };

  class IslandGraph {
  public:
    IslandGraph()
      : mIslandIndicesDirty(true) {}

    void add(Constraint& constraint);
    void remove(Constraint& constraint);
    void remove(PhysicsObject& obj);
    void clear();

    //Force wake up the island this object belongs to, if any. Needed for waking outside of constraints, like set position
    void wakeIsland(PhysicsObject& obj);
    //Store sleep state this frame, either Active or Inactive, and this will accumulate or reset inactive time
    void updateIslandState(IndexableKey islandKey, SleepState stateThisFrame, float dt);

    size_t islandCount();
    void getIsland(size_t index, IslandContents& result, bool fillInactive = false);

    bool validate();

  private:
    std::vector<IndexableKey>& _getIslandIndices();
    void _clearTraversed();
    bool _hasTraversedNode(IslandIndex node);
    bool _hasTraversedEdge(IslandIndex edge);
    IslandIndex _popToProcess();

    IslandIndex _getNode(PhysicsObject& obj);
    IslandIndex _getEdge(Constraint& constraint);
    Island& _getIslandFromNode(IslandIndex inIsland);

    void _createNewIsland(IslandIndex a, IslandIndex b, IslandIndex edge);
    void _addToIsland(IslandIndex hasIsland, IslandIndex toAdd, IslandIndex edge);
    void _mergeIslands(IslandIndex a, IslandIndex b, IslandIndex edge);

    void _removeIslandLeaf(IslandIndex inIsland, IslandIndex leaf);
    void _splitIsland(IslandIndex a, IslandIndex b);
    //Returns number of static nodes gathered
    size_t _gatherNodes(IslandIndex start, std::vector<IslandIndex>& container);
    void _removeIsland(IslandIndex a, IslandIndex b);
    //Find a nonstatic node from 'from' that can be a new root
    IslandIndex _findNewRoot(IslandIndex from);
    //Find and assign a new node to be the root of to's island starting from to, and searching his edges if he's static
    void _moveRoot(IslandIndex to);
    IndexableKey _newIsland(IslandIndex root);
    void _wakeIslandsWithStaticNode(IslandIndex staticIndex);

    StaticIndexable<IslandNode> mNodes;
    StaticIndexable<IslandEdge> mEdges;
    StaticIndexable<Island> mIslands;
    std::vector<IndexableKey> mIslandIndices;
    bool mIslandIndicesDirty;

    //Temporary containers used while traversing graph
    std::unordered_set<IslandIndex> mTraversedNodes;
    std::unordered_set<IslandIndex> mTraversedEdges;
    std::queue<IslandIndex> mToProcess;
    std::vector<IslandIndex> mGatheredNodes;

    //Handle mappings. IndexableKey is also IslandIndex for edges and nodes
    std::unordered_map<Handle, IslandIndex> mObjectToNode;
    std::unordered_map<Handle, IslandIndex> mConstraintToEdge;
  };
}