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

    IndexContainer(): mSize(0) {}

    size_t Size() const {
      return mSize;
    }

    void PushBack(IslandIndex obj);
    void Remove(size_t index);
    IslandIndex operator[](size_t index) const;

    void Clear();

  private:
    IslandIndex& Get(size_t index);

    IslandIndex mStatic[sMaxStaticSize];
    std::vector<IslandIndex> mDynamic;
    size_t mSize;
  };

  struct IslandNode {
    IslandNode();
    IslandNode(IslandIndex island)
      : mIsland(island) {}

    void RemoveEdge(IslandIndex edge);

    IndexableKey mIsland;
    IndexContainer mEdges;
  };

  struct IslandEdge {
    IslandEdge();
    IslandEdge(IslandIndex from, IslandIndex to, Constraint& constraint)
      : mFrom(from)
      , mTo(to)
      , mConstraint(&constraint) {}

    IslandIndex GetOther(IslandIndex index) const {
      return index == mFrom ? mTo : mFrom;
    }

    static void Link(IslandNode& a, IslandNode& b, IslandIndex edge);
    static void Unlink(IslandNode& a, IslandNode& b, IslandIndex edge);

    Constraint* mConstraint;
    IslandIndex mFrom;
    IslandIndex mTo;
  };

  //Not sure what I'll want to be tacking on here, so wrapping it in a struct
  struct IslandContents {
    void Clear() {
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

    void Add(IndexableKey islandKey, IslandNode& node);
    void Remove(IslandNode& node);
    void SetInactive(float dt);
    void SetActive(bool clearAwake = false);

    IslandIndex mRoot;
    size_t mSize;
    SleepState mSleepState;
    float mInactiveTime;
  };

  class IslandGraph {
  public:
    IslandGraph()
      : mIslandIndicesDirty(true) {}

    void Add(Constraint& constraint);
    void Remove(Constraint& constraint);
    void Remove(PhysicsObject& obj);
    void Clear();

    //Force wake up the island this object belongs to, if any. Needed for waking outside of constraints, like set position
    void WakeIsland(PhysicsObject& obj);
    //Store sleep state this frame, either Active or Inactive, and this will accumulate or reset inactive time
    void UpdateIslandState(IndexableKey islandKey, SleepState stateThisFrame, float dt);

    size_t IslandCount();
    void GetIsland(size_t index, IslandContents& result, bool fillInactive = false);

    bool Validate();

  private:
    std::vector<IndexableKey>& GetIslandIndices();
    void ClearTraversed();
    bool HasTraversedNode(IslandIndex node);
    bool HasTraversedEdge(IslandIndex edge);
    IslandIndex PopToProcess();

    IslandIndex GetNode(PhysicsObject& obj);
    IslandIndex GetEdge(Constraint& constraint);
    Island& GetIslandFromNode(IslandIndex inIsland);

    void CreateNewIsland(IslandIndex a, IslandIndex b, IslandIndex edge);
    void AddToIsland(IslandIndex hasIsland, IslandIndex toAdd, IslandIndex edge);
    void MergeIslands(IslandIndex a, IslandIndex b, IslandIndex edge);

    void RemoveIslandLeaf(IslandIndex inIsland, IslandIndex leaf);
    void SplitIsland(IslandIndex a, IslandIndex b);
    //Returns number of static nodes gathered
    size_t GatherNodes(IslandIndex start, std::vector<IslandIndex>& container);
    void RemoveIsland(IslandIndex a, IslandIndex b);
    //Find a nonstatic node from 'from' that can be a new root
    IslandIndex FindNewRoot(IslandIndex from);
    //Find and assign a new node to be the root of to's island starting from to, and searching his edges if he's static
    void MoveRoot(IslandIndex to);
    IndexableKey NewIsland(IslandIndex root);
    void WakeIslandsWithStaticNode(IslandIndex staticIndex);

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