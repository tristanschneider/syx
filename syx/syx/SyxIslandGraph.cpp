#include "Precompile.h"
#include "SyxIslandGraph.h"
#include "SyxConstraint.h"
#include "SyxPhysicsObject.h"

//#define VALIDATE_ISLAND SyxAssertError(Validate(), "Failed island validation");
#define VALIDATE_ISLAND

namespace Syx {
  static const IslandIndex sInvalidIslandIndex = std::numeric_limits<IslandIndex>::max();
  //Static objects can be in multiple islands to allow situations like multiple islands to be on one ground plane.
  //As such, static objects have this as their island index, as they have no 1 real island index
  static const IslandIndex sStaticNodeIndex = sInvalidIslandIndex - 1;

  float Island::sTimeToSleep = 0.5f;

  Island::Island()
    : mRoot(sInvalidIslandIndex)
    , mSize(0)
    , mInactiveTime(0.0f)
    , mSleepState(SleepState::Awake)
  {
  }

  Island::Island(IslandIndex root)
    : mRoot(root)
    , mSize(0)
    , mInactiveTime(0.0f)
    , mSleepState(SleepState::Awake)
  {
  }

  void Island::Add(IndexableKey islandKey, IslandNode& node) {
    //Tracking static node count is more trouble than it's worth, so don't increment size for them
    if(node.mIsland != sStaticNodeIndex) {
      ++mSize;
      //Nonstatic node, so it can hold the island's key
      node.mIsland = islandKey;
    }
  }

  void Island::Remove(IslandNode& node) {
    //Not tracking count for static nodes
    if(node.mIsland != sStaticNodeIndex) {
      SyxAssertError(node.mIsland != sInvalidIslandIndex, "Tried to remove not that wasn't in an island");
      node.mIsland = sInvalidIslandIndex;
      --mSize;
    }
  }

  void IndexContainer::PushBack(IslandIndex obj) {
    if(mSize < sMaxStaticSize)
      mStatic[mSize] = obj;
    else
      mDynamic.push_back(obj);
    ++mSize;
  }

  void IndexContainer::Remove(size_t index) {
    Get(index) = Get(mSize - 1);
    if(mSize > sMaxStaticSize)
      mDynamic.pop_back();
    --mSize;
  }

  IslandIndex IndexContainer::operator[](size_t index) const {
    if(index < sMaxStaticSize)
      return mStatic[index];
    return mDynamic[index - sMaxStaticSize];
  }

  IslandIndex& IndexContainer::Get(size_t index) {
    if(index < sMaxStaticSize)
      return mStatic[index];
    return mDynamic[index - sMaxStaticSize];
  }

  void IndexContainer::Clear() {
    mSize = 0;
    mDynamic.clear();
  }

  IslandNode::IslandNode()
    : mIsland(sInvalidIslandIndex) {}

  void IslandNode::RemoveEdge(IslandIndex edge) {
    //It is a linear search, but these containers should be pretty small, particularly since edges aren't added to static objects
    for(size_t i = 0; i < mEdges.Size(); ++i) {
      if(mEdges[i] == edge) {
        mEdges.Remove(i);
        break;
      }
    }
  }

  IslandEdge::IslandEdge()
    : mFrom(sInvalidIslandIndex)
    , mTo(sInvalidIslandIndex)
    , mConstraint(nullptr) {}

  void IslandEdge::Link(IslandNode& a, IslandNode& b, IslandIndex edge) {
    //We don't traverse static edges, so don't maintain their edges, as that's wasted effort, and could get rather large for objects like ground planes
    if(a.mIsland != sStaticNodeIndex)
      a.mEdges.PushBack(edge);
    if(b.mIsland != sStaticNodeIndex)
      b.mEdges.PushBack(edge);
  }

  void IslandEdge::Unlink(IslandNode& a, IslandNode& b, IslandIndex edge) {
    a.RemoveEdge(edge);
    b.RemoveEdge(edge);
  }

  void IslandGraph::Add(Constraint& constraint) {
    VALIDATE_ISLAND

    IslandIndex indexA = GetNode(*constraint.GetObjA());
    IslandIndex indexB = GetNode(*constraint.GetObjB());
    IslandNode& nodeA = mNodes[indexA];
    IslandNode& nodeB = mNodes[indexB];
    //No sense in starting an island with only static objects
    if(nodeA.mIsland == sStaticNodeIndex && nodeB.mIsland == sStaticNodeIndex)
      return;

    SyxAssertError(mConstraintToEdge.find(constraint.GetHandle()) == mConstraintToEdge.end(), "Duplicate constraint addition");
    IslandIndex edgeIndex = GetEdge(constraint);
    IslandEdge& edge = mEdges[edgeIndex];
    edge.mConstraint = &constraint;
    edge.mFrom = indexA;
    edge.mTo = indexB;
    IslandIndex aIsland = nodeA.mIsland;
    IslandIndex bIsland = nodeB.mIsland;

    SyxAssertError(aIsland != sStaticNodeIndex || bIsland != sStaticNodeIndex, "Static to static constraints not allowed");

    //Neither are in an island, start a new one
    if((aIsland == sInvalidIslandIndex || aIsland == sStaticNodeIndex) &&
      (bIsland == sInvalidIslandIndex || bIsland == sStaticNodeIndex)) {
      CreateNewIsland(indexA, indexB, edgeIndex);
    }
    //Already in the same island, just add new edge
    else if(aIsland == bIsland) {
      nodeA.mEdges.PushBack(edgeIndex);
      nodeB.mEdges.PushBack(edgeIndex);
    }
    //A doesn't have an island, B does, add A to B's island
    else if(aIsland == sInvalidIslandIndex || aIsland == sStaticNodeIndex) {
      AddToIsland(indexB, indexA, edgeIndex);
    }
    //B doesn't have an island, A does, add B to A's island
    else if(bIsland == sInvalidIslandIndex || bIsland == sStaticNodeIndex) {
      AddToIsland(indexA, indexB, edgeIndex);
    }
    //They're both in their own islands, merge them
    else {
      MergeIslands(indexA, indexB, edgeIndex);
    }

    //Islands could have changed, so don't use aIsland and bIsland
    //Something active collided with a potentially sleeping island. Wake it
    if(nodeA.mIsland != sStaticNodeIndex)
      mIslands[nodeA.mIsland].SetActive();
    else
      mIslands[nodeB.mIsland].SetActive();

    VALIDATE_ISLAND
  }

  void IslandGraph::CreateNewIsland(IslandIndex a, IslandIndex b, IslandIndex edge) {
    mIslandIndicesDirty = true;
    IslandNode& nodeA = mNodes[a];
    IslandNode& nodeB = mNodes[b];

    IslandIndex root = a;
    //Static nodes can't be roots. They can't both be static because there's an early out for that before this is called
    if(nodeA.mIsland == sStaticNodeIndex)
      root = b;

    IndexableKey islandKey = NewIsland(root);
    Island& island = mIslands[islandKey];
    island.Add(islandKey, nodeA);
    island.Add(islandKey, nodeB);

    IslandEdge::Link(nodeA, nodeB, edge);
  }

  void IslandGraph::AddToIsland(IslandIndex hasIsland, IslandIndex toAdd, IslandIndex edge) {
    IslandNode& islandNode = mNodes[hasIsland];
    IslandNode& addNode = mNodes[toAdd];

    Island& island = mIslands[islandNode.mIsland];
    island.Add(islandNode.mIsland, addNode);

    IslandEdge::Link(islandNode, addNode, edge);
  }

  void IslandGraph::MergeIslands(IslandIndex a, IslandIndex b, IslandIndex edge) {
    IslandIndex from = a;
    IslandIndex to = b;

    //Merge the smaller island into the bigger one
    if(GetIslandFromNode(a).mSize > GetIslandFromNode(b).mSize) {
      std::swap(from, to);
    }

    IslandNode& fromNode = mNodes[from];
    IslandNode& toNode = mNodes[to];
    Island& toIsland = mIslands[toNode.mIsland];
    IndexableKey toIslandId = toNode.mIsland;

    mIslands.Erase(fromNode.mIsland);
    mIslandIndicesDirty = true;

    ClearTraversed();
    mToProcess.push(from);

    //Walk all nodes in from and set their island to to's island
    while(!mToProcess.empty()) {
      IslandIndex nodeIndex = PopToProcess();
      //Add duplicates of static nodes to reference count the edges
      if(HasTraversedNode(nodeIndex))
        continue;

      IslandNode& node = mNodes[nodeIndex];
      if(node.mIsland == sStaticNodeIndex)
        continue;

      toIsland.Add(toIslandId, node);

      for(size_t i = 0; i < node.mEdges.Size(); ++i) {
        IslandIndex edgeIndex = node.mEdges[i];
        if(HasTraversedEdge(edgeIndex))
          continue;
        const IslandEdge& curEdge = mEdges[edgeIndex];
        mToProcess.push(curEdge.GetOther(nodeIndex));
      }
    }

    IslandEdge::Link(fromNode, toNode, edge);
  }

  void IslandGraph::Remove(Constraint& constraint) {
    VALIDATE_ISLAND

    auto edgeIt = mConstraintToEdge.find(constraint.GetHandle());
    if(edgeIt == mConstraintToEdge.end()) {
      SyxAssertError(constraint.GetObjA()->IsStatic() && constraint.GetObjB()->IsStatic(), "Removed constraint that didn't exist");
      return;
    }
    IslandIndex edgeIndex = edgeIt->second;
    IslandEdge& edge = mEdges[edgeIndex];
    IslandIndex indexA = edge.mFrom;
    IslandIndex indexB = edge.mTo;
    IslandNode& nodeA = mNodes[indexA];
    IslandNode& nodeB = mNodes[indexB];

    IslandEdge::Unlink(nodeA, nodeB, edgeIndex);
    mConstraintToEdge.erase(edgeIt);
    edge.mConstraint = nullptr;
    mEdges.Erase(edgeIndex);
    size_t aEdges = nodeA.mEdges.Size();
    size_t bEdges = nodeB.mEdges.Size();

    //Something like bottom of stack may have just been removed, wake island
    if(nodeA.mIsland != sStaticNodeIndex)
      mIslands[nodeA.mIsland].SetActive();
    else
      mIslands[nodeB.mIsland].SetActive();

    //If this was the only edge left in the island, remove it
    if(!aEdges && !bEdges) {
      RemoveIsland(indexA, indexB);
    }
    //A doesn't have any additional edges, it can be removed without splitting the island
    else if(!aEdges) {
      RemoveIslandLeaf(indexB, indexA);
    }
    //A doesn't have any additional edges, it can be removed without splitting the island
    else if(!bEdges) {
      RemoveIslandLeaf(indexA, indexB);
    }
    //They both have additional edges we need to traverse the whole dang thing to see if the island needs to be split
    else {
      SplitIsland(indexA, indexB);
    }

    VALIDATE_ISLAND
  }

  void IslandGraph::Remove(PhysicsObject& obj) {
    VALIDATE_ISLAND

    auto nodeIt = mObjectToNode.find(obj.GetHandle());
    if(nodeIt == mObjectToNode.end()) {
      return;
    }
    IslandIndex nodeIndex = nodeIt->second;
    IslandNode& node = mNodes[nodeIndex];

    while(node.mEdges.Size())
      Remove(*mEdges[node.mEdges[0]].mConstraint);

    mObjectToNode.erase(nodeIt);
    mNodes.Erase(nodeIndex);

    VALIDATE_ISLAND
  }

  void IslandGraph::RemoveIslandLeaf(IslandIndex inIsland, IslandIndex leaf) {
    IslandNode& inNode = mNodes[inIsland];
    Island& island = mIslands[inNode.mIsland];

    IslandNode oldRoot = mNodes[island.mRoot];
    IslandIndex oldRootIndex = island.mRoot;
    oldRootIndex = oldRootIndex;

    //If the leaf was the root, move it to the portion of the island that's sticking around
    if(island.mRoot == leaf)
      MoveRoot(inIsland);

    island.Remove(mNodes[leaf]);
  }

  void IslandGraph::SplitIsland(IslandIndex a, IslandIndex b) {
    Island* islandA = &GetIslandFromNode(a);

    //Gather all nodes that can be reached from the root
    size_t staticNodes = GatherNodes(islandA->mRoot, mGatheredNodes);

    //If all nodes can still be reached that means the island can remain as is
    //Island only tracks non-static nodes, so add them on when comparing
    if(mTraversedNodes.size() == islandA->mSize + staticNodes)
      return;

    //Find the other side to give it a new island. It's the one we haven't traversed
    IslandIndex otherSide = a;
    //Don't need to check both because we know we only traversed one of the two
    if(mTraversedNodes.find(a) != mTraversedNodes.end())
      otherSide = b;

    GatherNodes(otherSide, mGatheredNodes);

    //Find and construct new island root
    IslandIndex newRootIndex = FindNewRoot(mGatheredNodes.front());
    IndexableKey newIslandKey = NewIsland(newRootIndex);
    Island& newIsland = mIslands[newIslandKey];
    //Push could resize, so grab new pointer
    islandA = &GetIslandFromNode(a);

    //Point all nodes in the island at the new root
    for(IslandIndex curIndex : mGatheredNodes) {
      IslandNode& curNode = mNodes[curIndex];
      islandA->Remove(curNode);
      newIsland.Add(newIslandKey, curNode);
    }

    mIslandIndicesDirty = true;
  }

  size_t IslandGraph::GatherNodes(IslandIndex start, std::vector<IslandIndex>& container) {
    container.clear();
    ClearTraversed();

    size_t staticNodes = 0;
    //For each node in island
    mToProcess.push(start);
    while(!mToProcess.empty()) {
      IslandIndex nodeIndex = PopToProcess();
      if(HasTraversedNode(nodeIndex))
        continue;

      container.push_back(nodeIndex);

      const IslandNode& node = mNodes[nodeIndex];
      if(node.mIsland == sStaticNodeIndex) {
        ++staticNodes;
        continue;
      }

      //For each edge in node, push new constraints and the nodes they lead to
      for(size_t i = 0; i < node.mEdges.Size(); ++i) {
        IslandIndex edgeIndex = node.mEdges[i];
        if(HasTraversedEdge(edgeIndex))
          continue;
        const IslandEdge& edge = mEdges[edgeIndex];
        mToProcess.push(edge.GetOther(nodeIndex));
      }
    }
    return staticNodes;
  }

  void IslandGraph::RemoveIsland(IslandIndex a, IslandIndex b) {
    IslandNode& nodeA = mNodes[a];
    IslandNode& nodeB = mNodes[b];
    //Find the old island so we can remove it, but static nodes won't have it. They can't both be static because we don't allow static-static constraints
    IndexableKey islandKey = nodeA.mIsland == sStaticNodeIndex ? nodeB.mIsland : nodeA.mIsland;
    Island& island = mIslands[islandKey];

    island.Remove(nodeA);
    island.Remove(nodeB);
    SyxAssertError(island.mSize == 0, "Removed island that wasn't empty");
    mIslands.Erase(islandKey);
    mIslandIndicesDirty = true;
  }

  IslandIndex IslandGraph::FindNewRoot(IslandIndex from) {
    IslandIndex newRoot = from;
    IslandNode& toNode = mNodes[from];

    //If this is a static node we need to traverse edges to find a different root.
    //Doesn't need to be recursive because we don't allow static-static constraints
    if(toNode.mIsland == sStaticNodeIndex) {
      for(size_t i = 0; i < toNode.mEdges.Size(); ++i) {
        IslandEdge& edge = mEdges[toNode.mEdges[i]];
        IslandIndex nodeIndex = edge.GetOther(from);
        IslandNode& node = mNodes[nodeIndex];
        if(node.mIsland != sStaticNodeIndex) {
          newRoot = nodeIndex;
          break;
        }
      }
    }
    return newRoot;
  }

  void IslandGraph::MoveRoot(IslandIndex to) {
    IslandIndex newRoot = FindNewRoot(to);
    GetIslandFromNode(newRoot).mRoot = newRoot;
  }

  IndexableKey IslandGraph::NewIsland(IslandIndex root) {
    IndexableKey result = mIslands.Push(Island(root));
    SyxAssertError(result != sInvalidIslandIndex && result != sStaticNodeIndex, "Invalid island index created");
    return result;
  }

  void IslandGraph::Clear() {
    mNodes.Clear();
    mEdges.Clear();
    mIslands.Clear();
    mIslandIndices.clear();
    mObjectToNode.clear();
    mConstraintToEdge.clear();
    mIslandIndicesDirty = true;
  }

  size_t IslandGraph::IslandCount() {
    return GetIslandIndices().size();
  }

  void IslandGraph::GetIsland(size_t index, IslandContents& result, bool fillInactive) {
    IndexableKey islandKey = GetIslandIndices()[index];
    Island& island = mIslands[islandKey];
    IslandIndex root = mIslands[islandKey].mRoot;
    ClearTraversed();

    result.Clear();
    result.mIslandKey = islandKey;
    result.mSleepState = island.mSleepState;
    if(!fillInactive && island.mSleepState == SleepState::Inactive)
      return;

    //For each node in island
    mToProcess.push(root);
    while(!mToProcess.empty()) {
      IslandIndex nodeIndex = PopToProcess();
      if(HasTraversedNode(nodeIndex))
        continue;

      const IslandNode& node = mNodes[nodeIndex];
      if(node.mIsland == sStaticNodeIndex)
        continue;
      //For each edge in node, push new constraints and the nodes they lead to
      for(size_t i = 0; i < node.mEdges.Size(); ++i) {
        IslandIndex edgeIndex = node.mEdges[i];
        if(HasTraversedEdge(edgeIndex))
          continue;
        const IslandEdge& edge = mEdges[edgeIndex];
        result.mConstraints.push_back(edge.mConstraint);
        mToProcess.push(edge.GetOther(nodeIndex));
      }
    }
  }

  std::vector<IndexableKey>& IslandGraph::GetIslandIndices() {
    if(mIslandIndicesDirty)
      mIslands.GetIndices(mIslandIndices);
    mIslandIndicesDirty = false;
    return mIslandIndices;
  }

  void IslandGraph::ClearTraversed() {
    mTraversedEdges.clear();
    mTraversedNodes.clear();
  }

  bool IslandGraph::HasTraversedNode(IslandIndex node) {
    return !mTraversedNodes.insert(node).second;
  }

  bool IslandGraph::HasTraversedEdge(IslandIndex edge) {
    return !mTraversedEdges.insert(edge).second;
  }

  IslandIndex IslandGraph::PopToProcess() {
    IslandIndex result = mToProcess.front();
    mToProcess.pop();
    return result;
  }

  IslandIndex IslandGraph::GetNode(PhysicsObject& obj) {
    Handle handle = obj.GetHandle();
    auto it = mObjectToNode.find(handle);
    if(it != mObjectToNode.end())
      return it->second;

    //No existing node found, make new one
    IslandIndex newIndex = static_cast<IslandIndex>(mNodes.Push(IslandNode(obj.IsStatic() ? sStaticNodeIndex : sInvalidIslandIndex)));
    mObjectToNode[handle] = newIndex;
    return newIndex;
  }

  IslandIndex IslandGraph::GetEdge(Constraint& constraint) {
    Handle handle = constraint.GetHandle();
    auto it = mConstraintToEdge.find(handle);
    if(it != mConstraintToEdge.end())
      return it->second;

    //No existing constraint found, make new one
    IslandIndex newIndex = static_cast<IslandIndex>(mEdges.Push(IslandEdge(sInvalidIslandIndex, sInvalidIslandIndex, constraint)));
    mConstraintToEdge[handle] = newIndex;
    return newIndex;
  }

  Island& IslandGraph::GetIslandFromNode(IslandIndex inIsland) {
    IslandNode& nodeInIsland = mNodes[inIsland];
    return mIslands[nodeInIsland.mIsland];
  }

  bool IslandGraph::Validate() {
    std::vector<IndexableKey>& islands = GetIslandIndices();
    //Validate that both containers are in sync
    if(islands.size() != mIslands.Size())
      return false;

    //Validate that each node in an island points back at the right island and that the island has as many nodes as it thinks it does
    for(IndexableKey islandKey : islands) {
      Island& island = mIslands[islandKey];
      size_t staticNodes = GatherNodes(island.mRoot, mGatheredNodes);

      //Make sure island count matches traversed count
      if(island.mSize + staticNodes != mTraversedNodes.size())
        return false;

      for(IslandIndex inIslandIndex : mGatheredNodes) {
        IslandNode& curNode = mNodes[inIslandIndex];
        //Make sure each node is pointing at the island it's in, or is static
        if(curNode.mIsland != islandKey && curNode.mIsland != sStaticNodeIndex)
          return false;

        //Check for duplicate edges
        for(size_t i = 0; i < curNode.mEdges.Size(); ++i)
          for(size_t j = i + 1; j < curNode.mEdges.Size(); ++j)
            if(curNode.mEdges[i] == curNode.mEdges[j])
              return false;
      }
    }

    static std::unordered_set<Handle> constraints;
    static IslandContents contents;
    constraints.clear();
    contents.Clear();

    for(size_t i = 0; i < IslandCount(); ++i) {
      GetIsland(i, contents);
      contents.Clear();
      for(Constraint* c : contents.mConstraints) {
        if(!constraints.insert(c->GetHandle()).second)
          return false;
      }
    }

    return true;
  }

  void Island::SetInactive(float dt) {
    switch(mSleepState) {
      //If we're not asleep, accumulate inactive time and go to sleep if it's been long enough
      case SleepState::Active:
      case SleepState::Awake:
        mInactiveTime += dt;
        //Awake is the new state, we've been awake, now flip to the continuous active state
        mSleepState = SleepState::Active;
        if(mInactiveTime > sTimeToSleep)
          mSleepState = SleepState::Asleep;
        break;

      //If we were just put to sleep, now clear the new status and go to inactive
      case SleepState::Asleep:
        mSleepState = SleepState::Inactive;
      break;

      //If we're already inactive, nothing to do
      case SleepState::Inactive: break;
    }
  }

  void Island::SetActive(bool clearAwake) {
    switch(mSleepState) {
      //If we were inactive, reset inactive time and wake up
      case SleepState::Asleep:
      case SleepState::Inactive:
        mSleepState = SleepState::Awake;
        break;

      //If we were newly awakened, clear the new status to just active
      case SleepState::Awake:
        if(clearAwake)
          mSleepState = SleepState::Active;
        break;

      //If !clearAwake we want to reset to awake, as an island change happened that objects should be informed about
      case SleepState::Active:
        if(!clearAwake)
          mSleepState = SleepState::Awake;
        break;
    }
    //Regardless of state, reset inactive timer
    mInactiveTime = 0.0f;
  }

  void IslandGraph::WakeIsland(PhysicsObject& obj) {
    Handle handle = obj.GetHandle();
    auto it = mObjectToNode.find(handle);
    //If we didn't find it, it's not involved in any islands, so nothing to wake
    if(it == mObjectToNode.end())
      return;

    IslandIndex nodeIndex = it->second;
    IslandNode& node = mNodes[nodeIndex];
    switch(node.mIsland) {
      //Not in island, nothing to do
      case sInvalidIslandIndex:
        break;
      //Could be in multiple islands, so we need to find them all and wake them
      case sStaticNodeIndex:
        WakeIslandsWithStaticNode(nodeIndex);
        break;
      //Belongs to one island, update it
      default:
        mIslands[node.mIsland].SetActive();
        break;
    }
  }

  void IslandGraph::WakeIslandsWithStaticNode(IslandIndex staticIndex) {
    //Don't have a way to find all islands a static node belongs to, but this shouldn't happen often,
    //as it's setting position of a static object, at which point it probably shouldn't be static
    const std::vector<IndexableKey>& islands = GetIslandIndices();
    for(IndexableKey islandKey : islands) {
      Island& island = mIslands[islandKey];
      GatherNodes(island.mRoot, mGatheredNodes);
      //Wake any islands that contain this node
      if(std::find(mGatheredNodes.begin(), mGatheredNodes.end(), staticIndex) != mGatheredNodes.end())
        island.SetActive();
    }
  }

  void IslandGraph::UpdateIslandState(IndexableKey islandKey, SleepState stateThisFrame, float dt) {
    SyxAssertError(stateThisFrame == SleepState::Active || stateThisFrame == SleepState::Inactive, "Invalid state");
    Island& island = mIslands[islandKey];
    if(stateThisFrame == SleepState::Active)
      island.SetActive(true);
    else
      island.SetInactive(dt);
  }
}