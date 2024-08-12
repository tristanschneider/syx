#include "Precompile.h"
#include "IslandGraph.h"

namespace IslandGraph {
  namespace Debug {
    bool validateIslands(Graph& graph) {
      std::unordered_set<const Edge*> expectedEdges;
      std::unordered_set<const Node*> expectedNodes;
      std::unordered_set<const Node*> nodesInIsland;
      expectedEdges.reserve(graph.edges.size());
      expectedNodes.reserve(graph.nodes.size());
      auto returnNotValid = [](const char*) {
        __debugbreak();
        return false;
      };
      for(const Node& n : graph.nodes) {
        //All propagating nodes are expected, non-propagating may be found but not required
        if(n.propagation) {
          expectedNodes.insert(&n);
        }
      }
      for(const Edge& e : graph.edges) {
        if(!graph.nodes.isValid(e.nodeA) || !graph.nodes.isValid(e.nodeB)) {
          return returnNotValid("Invalid node index on edge");
        }
        const Node& na = graph.nodes[e.nodeA];
        const Node& nb = graph.nodes[e.nodeB];
        //Only edges between propagating nodes are expected to be found in islands
        if(na.propagation || nb.propagation) {
          expectedEdges.insert(&e);
        }
      }

      for(const Island& island : graph.islands) {
        uint32_t ei = island.edges;
        const IslandIndex islandIndex = static_cast<IslandIndex>(&island - graph.islands.values.data());
        while(ei != IslandGraph::INVALID) {
          if(!graph.edges.isValid(ei)) {
            return returnNotValid("Invalid edge on island");
          }

          //Validate the edge itself
          const Edge& e = graph.edges[ei];
          auto foundEdge = expectedEdges.find(&e);
          //Every edge should be expected and only found once
          if(foundEdge == expectedEdges.end()) {
            return returnNotValid("Unexpected edge in island");
          }
          //Erase from expected so it asserts if found again
          expectedEdges.erase(foundEdge);

          //Validate nodes in edge
          auto validateNode = [&](uint32_t nodeIndex) {
            if(!graph.nodes.isValid(nodeIndex)) {
              return returnNotValid("Invalid node in island");
            }
            const Node& node = graph.nodes[nodeIndex];
            //Node is expected to be found in a single island if it's a propagating node
            //Non-propagating nodes might share multiple islands
            if(node.propagation) {
              auto foundNode = expectedNodes.find(&node);
              if(foundNode == expectedNodes.end()) {
                return returnNotValid("Unexpected node in island");
              }
              nodesInIsland.insert(&node);
              if(node.islandIndex != islandIndex) {
                return returnNotValid("Incorrect island index on node");
              }
            }
            return true;
          };
          if(!validateNode(e.nodeA) || !validateNode(e.nodeB)) {
            return false;
          }

          ei = e.islandNext;
        }

        //Make sure that all the nodes found in edge traversal are also found in node traversal
        uint32_t ni = island.nodes;
        //Root is always expected and may not show up in any edges if this node is by itself in an island
        nodesInIsland.insert(&graph.nodes[ni]);
        while(ni != IslandGraph::INVALID) {
          if(!graph.nodes.isValid(ni)) {
            return returnNotValid("Invalid node in island");
          }
          const Node& n = graph.nodes[ni];
          if(n.propagation) {
            //Erase from node list so final count can be validated
            if(!nodesInIsland.erase(&n)) {
              return returnNotValid("Node in node list but not in any edges");
            }
            //Erase from expected so finding this node again in a different island would assert
            if(!expectedNodes.erase(&n)) {
              return returnNotValid("Node in island but not expected");
            }
          }

          ni = n.islandNext;
        }

        if(nodesInIsland.size()) {
          return returnNotValid("Node in edge list but not in node list");
        }
        nodesInIsland.clear();
      }

      if(expectedEdges.size()) {
        return returnNotValid("Edges missing from islands");
      }
      if(expectedNodes.size()) {
        return returnNotValid("Nodes missing from islands");
      }

      return true;
    }

    void rebuildAndValidateIslands(Graph& graph) {
      Graph temp = graph;
      rebuildIslands(graph);
      if(!validateIslands(graph)) {
        //Do it again and debugger can stop here and step through problem
        rebuildIslands(temp);
      }
    }
  }

  void addToLinkedList(uint32_t& root, uint32_t entryIndex, EdgeEntry& entry, uint32_t edgeIndex) {
    entry.edge = edgeIndex;
    entry.nextEntry = root;
    root = entryIndex;
  }

  uint32_t removeFromLinkedList(
    uint32_t& root,
    uint32_t nodeToRemove,
    gnx::VectorFreeList<EdgeEntry>& entries,
    const std::vector<Edge>& edges) {
    uint32_t current = root;
    uint32_t* last = &root;
    while(current != INVALID) {
      EdgeEntry& entry = entries.values[current];
      const Edge& edge = edges[entry.edge];
      if(edge.nodeA == nodeToRemove || edge.nodeB == nodeToRemove) {
        *last = entry.nextEntry;
        uint32_t result = entry.edge;
        entries.deleteIndex(current);
        return result;
      }
      last = &entry.nextEntry;
      current = entry.nextEntry;
    }
    return INVALID;
  }

  bool removeFromLinkedList(
    uint32_t& root,
    uint32_t edgeToRemove,
    gnx::VectorFreeList<EdgeEntry>& entries) {
    uint32_t current = root;
    uint32_t* last = &root;
    while(current != INVALID) {
      EdgeEntry& entry = entries.values[current];
      if(entry.edge == edgeToRemove) {
        *last = entry.nextEntry;
        entries.deleteIndex(current);
        return true;
      }
      last = &entry.nextEntry;
      current = entry.nextEntry;
    }
    return false;
  }

  constexpr uint32_t NOT_VISITED = std::numeric_limits<uint32_t>::max() - 2;

  bool populateIsland(Graph& graph, Island& island, IslandIndex islandIndex) {
    bool didChange = false;
    std::vector<uint32_t>& nodesTodo = graph.scratchBuffer;
    std::vector<uint32_t>& oldNoPropagateNodes = graph.scratchIslands;
    oldNoPropagateNodes.clear();
    const uint32_t islandRootIndex = island.nodes;
    const uint32_t oldCount = island.nodeCount;
    island.edgeCount = island.nodeCount = 0;
    island.edges = INVALID;
    island.nodes = INVALID;
    island.noPropagateNodes.swap(oldNoPropagateNodes);
    if(islandRootIndex == INVALID) {
      return didChange;
    }

    //If this was already traversed from a previous island skip it
    if(graph.visitedNodes[islandRootIndex]) {
      return didChange;
    }

    nodesTodo.push_back(islandRootIndex);
    Island& currentIsland = island;
    uint32_t* lastNode = &currentIsland.nodes;
    uint32_t* lastEdge = &currentIsland.edges;

    //For each node in the island
    while(nodesTodo.size()) {
      const size_t currentIndex = nodesTodo.back();
      Node& node = graph.nodes.values[currentIndex];
      nodesTodo.pop_back();
      //Nodes without propagation are only visited from others. So they'll show up in the island
      //if another node has an edge to it but edges between two non-propagating nodes won't
      if(graph.visitedNodes[currentIndex] || graph.nodes.isFree(currentIndex)) {
        continue;
      }

      if(!node.propagation) {
        //Sorted insert without duplicates
        auto it = std::lower_bound(island.noPropagateNodes.begin(), island.noPropagateNodes.end(), currentIndex);
        if(it == island.noPropagateNodes.end() || *it != currentIndex) {
          island.noPropagateNodes.insert(it, currentIndex);
        }
        continue;
      }
      //Add node to linked list for this island
      node.islandNext = *lastNode;
      graph.visitedNodes[currentIndex] = true;
      *lastNode = currentIndex;

      currentIsland.nodeCount++;

      //Skip entries for nodes that don't propagate
      if(node.propagation) {
        if(node.islandIndex != islandIndex) {
          node.islandIndex = islandIndex;
          didChange = true;
        }
        uint32_t currentEntry = node.edges;
        //Iterate over all edges connected to the current node
        while(currentEntry != INVALID) {
          const EdgeEntry& entry = graph.edgeEntries.values[currentEntry];
          Edge& edge = graph.edges.values[entry.edge];
          //Add edge to linked list if it hasn't been traversed already
          if(!graph.visitedEdges[entry.edge]) {
            //Add the other node for visitation
            nodesTodo.push_back(edge.nodeA == currentIndex ? edge.nodeB : edge.nodeA);
            currentIsland.edgeCount++;

            edge.islandNext = *lastEdge;
            graph.visitedEdges[entry.edge] = true;
            *lastEdge = entry.edge;
            lastEdge = &edge.islandNext;
          }
          currentEntry = entry.nextEntry;
        }
      }
      else {
        //No island ownership for nodes that don't propagate as they can be on the border of multiple islands
        node.islandIndex = INVALID_ISLAND;
      }
    }
    if(oldCount != island.nodeCount || oldNoPropagateNodes != island.noPropagateNodes) {
      didChange = true;
    }
    return didChange;
  }

  void rebuildIslands(Graph& graph) {
    graph.scratchBuffer.clear();

    graph.visitedNodes.clear();
    graph.visitedNodes.resize(graph.nodes.values.size(), false);
    graph.visitedEdges.clear();
    graph.visitedEdges.resize(graph.edges.values.size(), false);

    {
      const size_t islandCount = graph.islands.values.size();
      graph.publishedIslandEdgesChanged.clear();
      graph.publishedIslandEdgesChanged.resize(islandCount, false);
      graph.publishedIslandNodesChanged.clear();
      graph.publishedIslandNodesChanged.resize(islandCount, false);
    }

    //Iterate over each non-visited node and visit all in that island. This likely means a large amount
    //of skipped nodes after each edge traversal
    //for(auto islandRoot = graph.nodes.begin(); islandRoot != graph.nodes.end(); ++islandRoot) {
    for(size_t i = 0; i < graph.changedIslands.size(); ++i) {
      if(!graph.changedIslands[i]) {
        continue;
      }
      graph.changedIslands[i] = false;

      Island& island = graph.islands[i];
      //If the island is invalid then something unexpected happened with the change tracking to mark
      //an old island as changed. This is a bit of a hack since it indicates something wrong happened earlier
      if(graph.islands.isFree(i)) {
        continue;
      }

      const bool nodesChanged = populateIsland(graph, island, static_cast<IslandIndex>(i));
      //If island is now empty, add it to the free list if it isn't already
      if(!island.size()) {
        island.clear();
        graph.islands.deleteIndex(static_cast<IslandIndex>(i));
      }
      else {
        //Edges must have changed or this wouldn't have been marked as a changed island
        graph.publishedIslandEdgesChanged[i] = true;
        if(nodesChanged) {
          graph.publishedIslandNodesChanged[i] = true;
        }
      }
    }

    //Iterate over all newly created nodes. If they added an edge with an existing island they would have
    //been traversed above
    //Otherwise, they must form new islands
    for(uint32_t newNode : graph.newNodes) {
      if(graph.visitedNodes[newNode] || !graph.nodes[newNode].propagation) {
        continue;
      }
      const IslandIndex islandIndex = graph.islands.newIndex();
      Island& newIsland = graph.islands.values[islandIndex];
      newIsland.nodes = newNode;
      populateIsland(graph, newIsland, islandIndex);
      if(newIsland.size()) {
        if(!newIsland.userdata && graph.userdataFactory) {
          newIsland.userdata = graph.userdataFactory->create();
        }
        //Ensure there's a matching entry for the changed islands bitset.
        //No action needed if island was from the free list since it would already be non-changed
        if(islandIndex >= graph.changedIslands.size()) {
          assert(graph.islands.values.size() < INVALID_ISLAND);
          assert(graph.changedIslands.size() == islandIndex);
          graph.changedIslands.push_back(false);
          graph.publishedIslandEdgesChanged.push_back(true);
          graph.publishedIslandNodesChanged.push_back(true);
        }
        else {
          //These are new islands so everything is considered "changed"
          graph.publishedIslandEdgesChanged[islandIndex] = true;
          graph.publishedIslandNodesChanged[islandIndex] = true;
        }
      }
      else {
        //Didn't actually want to make this island after-all, put it back in the free list
        newIsland.clear();
        graph.islands.deleteIndex(islandIndex);
      }
    }
    graph.newNodes.clear();
  }

  void logChangedNode(Graph& graph, const Node& node) {
    if(node.islandIndex != INVALID_ISLAND) {
      graph.changedIslands[node.islandIndex] = true;
    }
  }

  void logAddedNode(Graph& graph, const Node& node) {
    logChangedNode(graph, node);
  }

  void logMaybeSplitIslandNode(Graph& graph, const Node& node) {
    if(node.islandIndex != INVALID_ISLAND) {
      graph.changedIslands[node.islandIndex] = true;
      const auto nodeIndex = static_cast<uint32_t>(&node - graph.nodes.values.data());
      graph.newNodes.push_back(nodeIndex);
    }
  }

  //Removing a node could split any number of islands from any of the edges on it
  //This only marks the island it was in for reevaluation, potentially reassigning the head
  //The caller then logs all the removed edges as a result
  //If this results in an empty island the next rebuildIslands will add it to the free list
  void logRemovedNode(Graph& graph, const Node& node) {
    if(node.islandIndex != INVALID_ISLAND) {
      graph.changedIslands[node.islandIndex] = true;
      const auto nodeIndex = static_cast<uint32_t>(&node - graph.nodes.values.data());
      //Reassign head if this was the start of the island
      if(graph.islands[node.islandIndex].nodes == nodeIndex) {
        graph.islands[node.islandIndex].nodes = node.islandNext;
      }
    }
  }

  //If an edge is added then traversal needs to visit the islands both are in
  void logAddedEdge(Graph& graph, const Node& a, const Node& b) {
    logChangedNode(graph, a);
    logChangedNode(graph, b);
  }

  //If an edge is removed traversal needs to visit the island both were in, which is presumably the same
  //It then also needs to visit both nodes in case this removal split the island in two
  void logRemovedEdge(Graph& graph, const Node& a, const Node& b) {
    logMaybeSplitIslandNode(graph, a);
    logMaybeSplitIslandNode(graph, b);
  }

  uint32_t addEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b) {
    auto mappingsA = graph.nodeMappings.find(a);
    auto mappingsB = graph.nodeMappings.find(b);
    if(mappingsA == graph.nodeMappings.end() || mappingsB == graph.nodeMappings.end()) {
      assert(false);
      return INVALID;
    }
    //Add the single edge definition
    Edge newEdge;
    newEdge.nodeA = mappingsA->second.node;
    newEdge.nodeB = mappingsB->second.node;
    const uint32_t edgeIndex = graph.edges.newIndex();
    graph.edges.values[edgeIndex] = newEdge;

    Node& nodeA = graph.nodes.values[newEdge.nodeA];
    Node& nodeB = graph.nodes.values[newEdge.nodeB];
    logAddedEdge(graph, nodeA, nodeB);

    //Create space for edge entries of A and B
    const size_t entryIndexA = graph.edgeEntries.newIndex();
    const size_t entryIndexB = graph.edgeEntries.newIndex();

    addToLinkedList(nodeA.edges, entryIndexA, graph.edgeEntries.values[entryIndexA], edgeIndex);
    addToLinkedList(nodeB.edges, entryIndexB, graph.edgeEntries.values[entryIndexB], edgeIndex);
    return edgeIndex;
  }

  EdgeUserdata addUnmappedEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b) {
    if(const uint32_t edgeIndex = addEdge(graph, a, b); edgeIndex != INVALID) {
      graph.edges.values[edgeIndex].data = static_cast<EdgeUserdata>(edgeIndex);
      return static_cast<EdgeUserdata>(edgeIndex);
    }
    return static_cast<EdgeUserdata>(INVALID);
  }

  void addEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b, const EdgeUserdata& edge) {
    if(const uint32_t edgeIndex = addEdge(graph, a, b); edgeIndex != INVALID) {
      graph.edges.values[edgeIndex].data = edge;
    }
  }

  void removeEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b) {
    auto mappingsA = graph.nodeMappings.find(a);
    auto mappingsB = graph.nodeMappings.find(b);
    if(mappingsA == graph.nodeMappings.end() || mappingsB == graph.nodeMappings.end()) {
      assert(false);
      return;
    }

    Node& nodeA = graph.nodes.values[mappingsA->second.node];
    //Remove the edge to B from the list in A
    const uint32_t edge = removeFromLinkedList(nodeA.edges, mappingsB->second.node, graph.edgeEntries, graph.edges.values);
    assert(edge != INVALID);
    Node& nodeB = graph.nodes.values[mappingsB->second.node];
    logRemovedEdge(graph, nodeA, nodeB);
    //Remove the entry pointing at the edge found in A from the list of entries in B
    [[maybe_unused]] const bool removed = removeFromLinkedList(nodeB.edges, edge, graph.edgeEntries);
    assert(removed);

    graph.edges.deleteIndex(edge);
  }

  void removeEdge(Graph& graph, const Graph::ConstEdgeIterator& it) {
    const Edge& edge = graph.edges[it.edge];
    Node& a = graph.nodes[edge.nodeA];
    Node& b = graph.nodes[edge.nodeB];
    logRemovedEdge(graph, a, b);
    [[maybe_unused]] const bool removedA = removeFromLinkedList(a.edges, it.edge, graph.edgeEntries);
    [[maybe_unused]] const bool removedB = removeFromLinkedList(b.edges, it.edge, graph.edgeEntries);
    assert(removedA && removedB);
  }

  void addNode(Graph& graph, const NodeUserdata& data, IslandPropagationMask propagation) {
    const size_t newIndex = graph.nodes.newIndex();
    Node& node = graph.nodes.values[newIndex];
    node = {};
    node.data = data;
    node.propagation = propagation;
    NodeMappings mappings;
    mappings.node = newIndex;
    graph.nodeMappings[data] = mappings;
    graph.newNodes.push_back(newIndex);
  }

  void removeNode(Graph& graph, const NodeUserdata& data) {
    auto mappings = graph.nodeMappings.find(data);
    if(mappings == graph.nodeMappings.end()) {
      assert(false);
      return;
    }
    const uint32_t toRemove = mappings->second.node;
    graph.nodeMappings.erase(mappings);

    Node& node = graph.nodes.values[toRemove];
    logRemovedNode(graph, node);
    uint32_t current = node.edges;
    //Go through all edges this was connected to and remove them from the other
    while(current != INVALID) {
      EdgeEntry& entry = graph.edgeEntries[current];
      Edge& edge = graph.edges[entry.edge];

      //Remove entry for edge in other object
      const uint32_t other = edge.nodeA == toRemove ? edge.nodeB : edge.nodeA;
      Node& otherNode = graph.nodes[other];
      //Any edge outgoing from the removed node might need to split into a new island, mark them for evaluation
      logMaybeSplitIslandNode(graph, otherNode);
      removeFromLinkedList(otherNode.edges, entry.edge, graph.edgeEntries);

      //Remove the edge itself
      graph.edges.deleteIndex(entry.edge);

      //Remove the edge in the node that's being removed
      uint32_t entryToRemove = current;
      current = entry.nextEntry;
      graph.edgeEntries.deleteIndex(entryToRemove);
    }

    //All edges have been removed, remove the node itself
    graph.nodes.deleteIndex(toRemove);
  }

  void notifyNodeChanged(Graph& graph, Graph::NodeIterator it) {
    if(it != graph.nodesEnd()) {
      logChangedNode(graph, graph.nodes[it.node]);
    }
  }

  void setPropagation(Graph& graph, Graph::NodeIterator it, IslandPropagationMask propagation) {
    if(it != graph.nodesEnd()) {
      Node& node = graph.nodes[it.node];
      //If this is going from a propagating node to non-propagating it can no longer be the root
      //of any islands nor store an island index
      if(node.propagation && !propagation) {
        //This will mark as changed and move the head of the island
        logRemovedNode(graph, node);
        uint32_t current = node.edges;
        //Go through all edges this was connected to and notify potential split
        while(current != INVALID) {
          EdgeEntry& entry = graph.edgeEntries[current];
          Edge& edge = graph.edges[entry.edge];

          //Remove entry for edge in other object
          const uint32_t other = edge.nodeA == it.node ? edge.nodeB : edge.nodeA;
          Node& otherNode = graph.nodes[other];
          //Any edge outgoing from the changed node might need to split into a new island, mark them for evaluation
          logMaybeSplitIslandNode(graph, otherNode);
          current = entry.nextEntry;
        }
      }
      //For any other propagation changes they can get picked up by marking as changed
      else {
        notifyNodeChanged(graph, it);
      }
      node.propagation = propagation;
    }
  }

  Graph::NodeIterator Graph::findNode(const NodeUserdata& node) {
    auto it = nodeMappings.find(node);
    return { this, it != nodeMappings.end() ? it->second.node : INVALID };
  }

  Graph::ConstNodeIterator Graph::findNode(const NodeUserdata& node) const {
    auto it = nodeMappings.find(node);
    return { this, it != nodeMappings.end() ? it->second.node : INVALID };
  }

  Graph::NodePairEdgeIterator Graph::findEdge(const NodeUserdata& a, const NodeUserdata& b) const {
    //The edge will be in the list of A and B, arbitrarily look in A
    //If this is used often enough it may be worth storing the size so the smaller one can be traversed
    auto mapping = nodeMappings.find(a);
    auto mappingB = nodeMappings.find(b);
    if(mapping != nodeMappings.end() && mappingB != nodeMappings.end()) {
      const Node& node = nodes[mapping->second.node];
      NodePairEdgeIterator result{ this, node.edges, mappingB->second.node };
      if(result.edgeEntry != INVALID && !result.isMatchingEdge()) {
        ++result;
      }
      return result;
    }
    return edgesEnd();
  }
}