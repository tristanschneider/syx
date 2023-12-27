#include "Precompile.h"
#include "IslandGraph.h"

namespace IslandGraph {
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

  void rebuildIslands(Graph& graph) {
    for(Node& node : graph.nodes) {
      node.islandNext = NOT_VISITED;
    }
    for(Edge& edge : graph.edges) {
      edge.islandNext = NOT_VISITED;
    }
    graph.islands.clear();
    graph.scratchBuffer.clear();
    std::vector<uint32_t>& nodesTodo = graph.scratchBuffer;

    //Iterate over each non-visited node and visit all in that island. This likely means a large amount
    //of skipped nodes after each edge traversal
    for(auto islandRoot = graph.nodes.begin(); islandRoot != graph.nodes.end(); ++islandRoot) {
      //If this was already traversed from a previous island skip it
      if(islandRoot->islandNext != NOT_VISITED) {
        continue;
      }

      const uint32_t current = graph.nodes.rawIndex(islandRoot);
      nodesTodo.push_back(current);
      Island currentIsland;
      uint32_t* lastNode = &currentIsland.nodes;
      uint32_t* lastEdge = &currentIsland.edges;

      //For each node in the island
      while(nodesTodo.size()) {
        const size_t currentIndex = nodesTodo.back();
        Node& node = graph.nodes.values[currentIndex];
        nodesTodo.pop_back();
        //Nodes without propagation are only visited from others. So they'll show up in the island
        //if another node has an edge to it but edges between two non-propagating nodes won't
        if(node.islandNext != NOT_VISITED || !node.propagation) {
          continue;
        }
        //Add node to linked list for this island
        node.islandNext = *lastNode;
        *lastNode = currentIndex;

        currentIsland.nodeCount++;

        //Skip entries for nodes that don't propagate
        if(node.propagation) {
          uint32_t currentEntry = node.edges;
          //Iterate over all edges connected to the current node
          while(currentEntry != INVALID) {
            const EdgeEntry& entry = graph.edgeEntries.values[currentEntry];
            Edge& edge = graph.edges.values[entry.edge];
            //Add edge to linked list if it hasn't been traversed already
            if(edge.islandNext == NOT_VISITED) {
              //Add the other node for visitation
              nodesTodo.push_back(edge.nodeA == currentIndex ? edge.nodeB : edge.nodeA);
              currentIsland.edgeCount++;

              edge.islandNext = *lastEdge;
              *lastEdge = entry.edge;
              lastEdge = &edge.islandNext;
            }
            currentEntry = entry.nextEntry;
          }
        }
      }

      //No more nodes traversable from the current root, submit island and find next root
      //Skip it if there are no nodes, meaning it contained only non-propagating nodes
      if(currentIsland.nodeCount) {
        graph.islands.push_back(currentIsland);
      }
    }
  }

  void addEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b, const EdgeUserdata& edge) {
    auto mappingsA = graph.nodeMappings.find(a);
    auto mappingsB = graph.nodeMappings.find(b);
    if(mappingsA == graph.nodeMappings.end() || mappingsB == graph.nodeMappings.end()) {
      assert(false);
      return;
    }
    //Add the single edge definition
    Edge newEdge;
    newEdge.data = edge;
    newEdge.nodeA = mappingsA->second.node;
    newEdge.nodeB = mappingsB->second.node;
    const uint32_t edgeIndex = graph.edges.newIndex();
    graph.edges.values[edgeIndex] = newEdge;

    Node& nodeA = graph.nodes.values[newEdge.nodeA];
    Node& nodeB = graph.nodes.values[newEdge.nodeB];

    //Create space for edge entries of A and B
    const size_t entryIndexA = graph.edgeEntries.newIndex();
    const size_t entryIndexB = graph.edgeEntries.newIndex();

    addToLinkedList(nodeA.edges, entryIndexA, graph.edgeEntries.values[entryIndexA], edgeIndex);
    addToLinkedList(nodeB.edges, entryIndexB, graph.edgeEntries.values[entryIndexB], edgeIndex);
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
    //Remove the entry pointing at the edge found in A from the list of entries in B
    [[maybe_unused]] const bool removed = removeFromLinkedList(nodeB.edges, edge, graph.edgeEntries);
    assert(removed);

    graph.edges.deleteIndex(edge);
  }

  void removeEdge(Graph& graph, const Graph::EdgeIterator& it) {
    const Edge& edge = graph.edges[it.edge];
    Node& a = graph.nodes[edge.nodeA];
    Node& b = graph.nodes[edge.nodeB];
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
    uint32_t current = node.edges;
    //Go through all edges this was connected to and remove them from the other
    while(current != INVALID) {
      EdgeEntry& entry = graph.edgeEntries[current];
      Edge& edge = graph.edges[entry.edge];

      //Remove entry for edge in other object
      const uint32_t other = edge.nodeA == toRemove ? edge.nodeB : edge.nodeA;
      Node& otherNode = graph.nodes[other];
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

    Graph::EdgeIterator Graph::findEdge(const NodeUserdata& a, const NodeUserdata& b) {
      const Graph* self = this;
      Graph::ConstEdgeIterator it = self->findEdge(a, b);
      return { this, it.edge };
    }

    Graph::NodeIterator Graph::findNode(const NodeUserdata& node) {
      auto it = nodeMappings.find(node);
      return { this, it != nodeMappings.end() ? it->second.node : INVALID };
    }

    Graph::ConstNodeIterator Graph::findNode(const NodeUserdata& node) const {
      auto it = nodeMappings.find(node);
      return { this, it != nodeMappings.end() ? it->second.node : INVALID };
    }

    Graph::ConstEdgeIterator Graph::findEdge(const NodeUserdata& a, const NodeUserdata& b) const {
      //The edge will be in the list of A and B, arbitrarily look in A
      //If this is used often enough it may be worth storing the size so the smaller one can be traversed
      auto mapping = nodeMappings.find(a);
      auto mappingB = nodeMappings.find(b);
      if(mapping != nodeMappings.end() && mappingB != nodeMappings.end()) {
        const Node& node = nodes[mapping->second.node];
        //Iterate over all edges from A
        uint32_t currentEntry = node.edges;
        while(currentEntry != INVALID) {
          const EdgeEntry& entry = edgeEntries[currentEntry];
          const Edge& edge = edges[entry.edge];
          //The edge has A in it since it's in the list of A's edges so it's only necessary to find B
          if(edge.nodeA == mappingB->second.node || edge.nodeB == mappingB->second.node) {
            //B was found, return this edge
            return { this, entry.edge };
          }
          //Nothing found, continue down the list
          currentEntry = entry.nextEntry;
        }
      }
      return edgesEnd();
    }
}