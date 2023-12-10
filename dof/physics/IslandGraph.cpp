#include "Precompile.h"
#include "IslandGraph.h"

namespace IslandGraph {
  template<class T>
  size_t createWithFreeList(std::vector<T>& values, std::vector<uint32_t>& freeList) {
    if(freeList.empty()) {
      const size_t newIndex = values.size();
      values.emplace_back();
      return newIndex;
    }
    size_t freeIndex = freeList.back();
    freeList.pop_back();
    return freeIndex;
  }

  template<class T>
  void deleteWithFreeList(uint32_t index, std::vector<T>& values, std::vector<uint32_t>& freeList) {
    values[index].markAsFree();
    freeList.push_back(index);
  }

  Graph::EdgeIterator Graph::findEdge(const NodeUserdata& a, const NodeUserdata& b) {

  }

  //When the propagation mask doesn't match copies of nodes are added to islands.
  //In this case the island index refers to the original not the copy
  //If this is from a copy it's possible that an instance also exists in the destination, so check first
  void tryAddNonPropagatingNode(Island& island, uint32_t nodeIndex) {
    if(auto it = std::find(island.nodes.begin(), island.nodes.end(), nodeIndex); it == island.nodes.end()) {
      island.nodes.push_back(nodeIndex);
    }
  }

  void mergeIslands(
    uint32_t source,
    uint32_t destination,
    std::vector<Node>& nodes,
    std::unordered_map<NodeUserdata, Graph::NodeMappings>& nodeMappings,
    std::vector<Island>& islands,
    std::vector<uint32_t>& freeList
  ) {
    Island& sourceIsland = islands[source];
    Island& destinationIsland = islands[destination];
    //Copy over all the edges
    destinationIsland.edges.insert(destinationIsland.edges.end(), sourceIsland.edges.begin(), sourceIsland.edges.end());
    //Copy over all the nodes and update the mappings
    for(uint32_t node : sourceIsland.nodes) {
      Node& n = nodes[node];
      //When the propagation mask doesn't match copies of nodes are added to islands.
      //In this case the island index refers to the original not the copy
      //If this is from a copy it's possible that an instance also exists in the destination, so check first
      if(n.islandIndex != source) {
        //Add it to the destination if it isn't already there but leave the mappings unchanged
        tryAddNonPropagatingNode(destinationIsland, node);
      }
      //If this is a normal node that belonged to the island duplicates shouldn't be possible so copy it over and update the mapping
      else {
        n.islandIndex = destination;
        //TODO: is the map lookup worth having to update here?
        if(auto it = nodeMappings.find(n.data); it != nodeMappings.end()) {
          it->second.island = destination;
        }
        else {
          assert(false);
        }
        destinationIsland.nodes.push_back(node);
      }
    }

    deleteWithFreeList(source, islands, freeList);
  }

  void mergeIslandsAndAddEdge(
    Graph::NodeMappings& source,
    const Graph::NodeMappings& destination,
    std::vector<Node>& nodes,
    std::unordered_map<NodeUserdata, Graph::NodeMappings>& nodeMappings,
    uint32_t edgeIndex,
    std::vector<Island>& islands,
    std::vector<uint32_t>& freeList
  ) {
    //Migrate the contents of the island and delete it
    mergeIslands(source.island, destination.island, nodes, nodeMappings, islands, freeList);
    //Add the edge, the nodes are already both in the island after the merge
    islands[destination.island].edges.push_back(edgeIndex);
  }

  void addEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b, const EdgeUserdata& edge) {
    auto mappingsA = graph.nodeMappings.find(a);
    auto mappingsB = graph.nodeMappings.find(b);
    if(mappingsA == graph.nodeMappings.end() || mappingsB == graph.nodeMappings.end()) {
      assert(false);
      return;
    }

    uint32_t edgeIndex = createWithFreeList(graph.edges, graph.edgeFreeList);
    Edge& newEdge = graph.edges[edgeIndex];
    newEdge.edge.indices.first = mappingsA->second.node;
    newEdge.edge.indices.second = mappingsB->second.node;

    Island& iA = graph.islands[mappingsA->second.island];
    Island& iB = graph.islands[mappingsB->second.island];
    //If they're already in the same island, add the edge to the existing island
    if(&iA == &iB) {
      //Since they're already both in the island, no need to add to nodes list
      iA.edges.push_back(edgeIndex);
    }
    //If they're both in different islands that should merge, merge them
    else if(iA.propagation == iB.propagation) {
      //Merge the smaller into the bigger and add the new edge
      if(iA.edges.size() > iB.edges.size()) {
        mergeIslandsAndAddEdge(mappingsB->second, mappingsA->second, graph.nodes, graph.nodeMappings, edgeIndex, graph.islands, graph.islandFreeList);
      }
      else {
        mergeIslandsAndAddEdge(mappingsA->second, mappingsB->second, graph.nodes, graph.nodeMappings, edgeIndex, graph.islands, graph.islandFreeList);
      }
    }
    //If they're in islands that shouldn't merge, add to both
    else {
      tryAddNonPropagatingNode(iA, mappingsB->second.node);
      tryAddNonPropagatingNode(iB, mappingsA->second.node);
    }
  }

  struct IslandSplitContext {
    struct NodeInfo {
      //Index of another node this connects to
      uint32_t connectedNodeIndex{};
      //Index of next NodeInfo object indicating this object has an edge to another object
      uint32_t next{};
    };
    Island resultLeft;
    Island resultRight;
    //Graph node index to infoEntries index
    std::unordered_map<uint32_t, uint32_t> nodeGraph;
    std::vector<NodeInfo> infoEntries;
    std::vector<uint32_t> edgesTodo;
    std::unordered_set<uint32_t> traversed;
    uint32_t removedEdge{};
  };

  struct Graph2 {
    static constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();
    struct Node {
      NodeUserdata data{};
      IslandPropagationMask propagation{};
      //Root of linked list of EdgeEntry
      uint32_t edges{ INVALID };
      uint32_t islandNext{ INVALID };
    };
    //This is the element of a linked list for all edges going out of a single node
    struct EdgeEntry {
      //The edge
      uint32_t edge{};
      //The next EdgeEntry in the list
      uint32_t nextEntry{};
    };
    //These are unique edges, so while there may be multiple edge entries for pairs of objects
    //there is only one edge, which is why it holds the userdata
    struct Edge {
      uint32_t nodeA{};
      uint32_t nodeB{};
      EdgeUserdata data{};
      uint32_t islandNext{ INVALID };
    };
    struct NodeMappings {
      uint32_t node{};
    };
    struct Island {
      uint32_t nodes{};
      uint32_t edges{};
    };

    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<EdgeEntry> edgeEntries;
    std::unordered_map<NodeUserdata, NodeMappings> nodeMappings;
    std::vector<uint32_t> nodeFreeList;
    std::vector<uint32_t> edgeFreeList;
    std::vector<uint32_t> entryFreeList;
    std::vector<Island> islands;
  };

  void addToLinkedList(uint32_t& root, uint32_t entryIndex, Graph2::EdgeEntry& entry, uint32_t edgeIndex) {
    entry.edge = edgeIndex;
    entry.nextEntry = root;
    root = entryIndex;
  }

  uint32_t removeFromLinkedList(
    uint32_t& root,
    uint32_t nodeToRemove,
    std::vector<Graph2::EdgeEntry>& entries,
    std::vector<uint32_t>& entriesFreeList,
    const std::vector<Graph2::Edge>& edges) {
    uint32_t current = root;
    uint32_t* last = &root;
    while(current != Graph2::INVALID) {
      Graph2::EdgeEntry& entry = entries[current];
      const Graph2::Edge& edge = edges[entry.edge];
      if(edge.nodeA == nodeToRemove || edge.nodeB == nodeToRemove) {
        *last = entry.nextEntry;
        uint32_t result = entry.edge;
        deleteWithFreeList(current, entries, entriesFreeList);
        return result;
      }
      last = &entry.nextEntry;
      current = entry.nextEntry;
    }
  }

  constexpr uint32_t NOT_VISITED = std::numeric_limits<uint32_t>::max() - 1;

  void rebuildIslands(Graph2& graph) {
    for(Graph2::Node& node : graph.nodes) {
      node.islandNext = NOT_VISITED;
    }
    for(Graph2::Edge& edge : graph.edges) {
      edge.islandNext = NOT_VISITED;
    }
    graph.islands.clear();
    std::vector<uint32_t> nodesTodo;
    uint32_t current = 0;
    //Find first used node
    while(current < graph.nodes.size() && graph.nodes[current].isFree()) {
      nodesTodo.push_back(current);
      Graph2::Island currentIsland;
      uint32_t* lastNode = &currentIsland.nodes;
      uint32_t* lastEdge = &currentIsland.edges;

      while(nodesTodo.size()) {
        const size_t currentIndex = nodesTodo.back();
        Graph2::Node& node = graph.nodes[currentIndex];
        nodesTodo.pop_back();
        if(node.islandNext != NOT_VISITED) {
          continue;
        }
        //Add node to linked list for this island
        node.islandNext = *lastNode;
        *lastNode = currentIndex;

        //Skip entries for nodes that don't propagate
        if(node.propagation) {
          uint32_t currentEntry = node.edges;
          //Iterate over all edges connected to the current node
          while(currentEntry != Graph2::INVALID) {
            const Graph2::EdgeEntry& entry = graph.edgeEntries[currentEntry];
            Graph2::Edge& edge = graph.edges[entry.edge];
            //Add edge to linked list if it hasn't been traversed already
            if(edge.islandNext == NOT_VISITED) {
              //Add the other node for visitation
              nodesTodo.push_back(edge.nodeA == currentIndex ? edge.nodeB : edge.nodeA);

              edge.islandNext = *lastEdge;
              *lastEdge = entry.edge;
              lastEdge = &edge.islandNext;
            }
            currentEntry = entry.nextEntry;
          }
        }

        graph.islands.push_back(currentIsland);
      }
    }
  }

  void addEdge(Graph2& graph, const NodeUserdata& a, const NodeUserdata& b, const EdgeUserdata& edge) {
    auto mappingsA = graph.nodeMappings.find(a);
    auto mappingsB = graph.nodeMappings.find(b);
    if(mappingsA == graph.nodeMappings.end() || mappingsB == graph.nodeMappings.end()) {
      assert(false);
      return;
    }
    //Add the single edge definition
    Graph2::Edge newEdge;
    newEdge.data = edge;
    newEdge.nodeA = mappingsA->second.node;
    newEdge.nodeB = mappingsB->second.node;
    const uint32_t edgeIndex = static_cast<uint32_t>(graph.edges.size());
    graph.edges.push_back(newEdge);

    Graph2::Node& nodeA = graph.nodes[newEdge.nodeA];
    Graph2::Node& nodeB = graph.nodes[newEdge.nodeB];

    //Create space for edge entries of A and B
    const size_t entryIndexA = createWithFreeList(graph.edgeEntries, graph.entryFreeList);
    const size_t entryIndexB = createWithFreeList(graph.edgeEntries, graph.entryFreeList);

    addToLinkedList(nodeA.edges, entryIndexA, graph.edgeEntries[entryIndexA], edgeIndex);
    addToLinkedList(nodeB.edges, entryIndexB, graph.edgeEntries[entryIndexB], edgeIndex);
  }

  void removeEdge(Graph2& graph, const NodeUserdata& a, const NodeUserdata& b) {
    auto mappingsA = graph.nodeMappings.find(a);
    auto mappingsB = graph.nodeMappings.find(b);
    if(mappingsA == graph.nodeMappings.end() || mappingsB == graph.nodeMappings.end()) {
      assert(false);
      return;
    }

    Graph2::Node& nodeA = graph.nodes[mappingsA->second.node];
    uint32_t edge = removeFromLinkedList(nodeA.edges, mappingsA->second.node, graph.edgeEntries, graph.entryFreeList, graph.edges);
    Graph2::Node& nodeB = graph.nodes[mappingsB->second.node];
    [[maybe_unused]] uint32_t edgeCheck = removeFromLinkedList(nodeA.edges, mappingsB->second.node, graph.edgeEntries, graph.entryFreeList, graph.edges);
    assert(edge == edgeCheck);

    deleteWithFreeList(edge, graph.edges, graph.edgeFreeList);
  }

  void addNode(Graph2& graph, const NodeUserdata& data, IslandPropagationMask propagation) {
    const size_t newIndex = createWithFreeList(graph.nodes, graph.nodeFreeList);
    Graph2::Node& node = graph.nodes[newIndex];
    node.data = data;
    node.propagation = propagation;
    Graph2::NodeMappings mappings;
    mappings.node = newIndex;
    graph.nodeMappings[data] = mappings;
  }

  void removeNode(Graph2& graph, const NodeUserdata& data) {
    auto mappings = graph.nodeMappings.find(data);
    if(mappings == graph.nodeMappings.end()) {
      assert(false);
      return;
    }
    const uint32_t toRemove = mappings->second.node;
    Graph2::Node& node = graph.nodes[toRemove];
    uint32_t current = node.edges;
    //Go through all edges this was connected to and remove them from the other
    while(current != Graph2::INVALID) {
      Graph2::EdgeEntry& entry = graph.edgeEntries[current];
      Graph2::Edge& edge = graph.edges[entry.edge];

      //Remove entry for edge in other object
      const uint32_t other = edge.nodeA == toRemove ? edge.nodeB : edge.nodeA;
      Graph2::Node& otherNode = graph.nodes[other];
      removeFromLinkedList(otherNode.edges, toRemove, graph.edgeEntries, graph.entryFreeList, graph.edges);

      //Remove the edge itself
      deleteWithFreeList(entry.edge, graph.edges, graph.edgeFreeList);

      //Remove the edge in the node that's being removed
      uint32_t entryToRemove = current;
      current = entry.nextEntry;
      deleteWithFreeList(entryToRemove, graph.edgeEntries, graph.edgeFreeList);
    }

    //All edges have been removed, remove the node itself
    deleteWithFreeList(toRemove, graph.nodes, graph.nodeFreeList);
  }

  void trySplitIsland(
    const Island& island,
    IslandSplitContext& context,
    uint32_t splitEdge,
    const std::vector<Edge>& edges,
    const std::vector<Node>& nodes) {
    constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();
    //Dummy entry so zero is invalid
    context.infoEntries.push_back({});

    //Fill a map to describe the connectivity of the nodes
    for(uint32_t edgeIndex : island.edges) {
      if(edgeIndex != splitEdge) {
        const Edge& e = edges[edgeIndex];
        {
          const uint32_t fromIndex = e.edge.indices.first;
          const uint32_t toIndex = e.edge.indices.second;
          auto& info = context.nodeGraph[fromIndex];
          IslandSplitContext::NodeInfo newInfo;
          newInfo.connectedNodeIndex = toIndex;
          //Add to head of linked list
          newInfo.next = info;
          const uint32_t newIndex = static_cast<uint32_t>(context.infoEntries.size());
          info = newIndex;
          context.infoEntries.emplace_back(newInfo);
        }
        {
          const uint32_t fromIndex = e.edge.indices.second;
          const uint32_t toIndex = e.edge.indices.first;
          auto& info = context.nodeGraph[fromIndex];
          IslandSplitContext::NodeInfo newInfo;
          newInfo.connectedNodeIndex = toIndex;
          //Add to head of linked list
          newInfo.next = info;
          const uint32_t newIndex = static_cast<uint32_t>(context.infoEntries.size());
          info = newIndex;
          context.infoEntries.emplace_back(newInfo);
        }
      }
    }

    //Traverse the graph on one side of the split and see if it still spans the entire island
    context.edgesTodo.push_back(1);
    while(context.edgesTodo.size()) {
      const uint32_t infoEntry = context.edgesTodo.back();
      context.edgesTodo.pop_back();
      if(!context.traversed.insert(infoEntry).second) {
        continue;
      }
      const auto& info = context.infoEntries[infoEntry];
      uint32_t connected = info.next;
      //Add the others connected to this
      context.edgesTodo.push_back(info.next);

      while(connected) {
        context.edgesTodo.push_back(connected);
        connected = context.infoEntries[connected].next;
      }
      context.edgesTodo.push_back(context[infoEntry].
    }
  }

  void removeEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b) {
    auto mappingsA = graph.nodeMappings.find(a);
    auto mappingsB = graph.nodeMappings.find(b);
    if(mappingsA == graph.nodeMappings.end() || mappingsB == graph.nodeMappings.end()) {
      assert(false);
      return;
    }

    //If the islands match there is no special duplication, see if this edge removal should split the island
    if(mappingsA->second.island == mappingsB->second.island) {

    }
  }

  void addNode(Graph& graph, const NodeUserdata& data, IslandPropagationMask propagation) {
    const size_t islandIndex = createWithFreeList(graph.islands, graph.islandFreeList);
    const size_t nodeIndex = createWithFreeList(graph.nodes, graph.nodeFreeList);
    Node& node = graph.nodes[nodeIndex];
    node.data = data;
    node.islandIndex = islandIndex;
    graph.nodeToIsland[data] = islandIndex;
    //Every node starts with its own island which are then merged as edges are added
    //It may save some memory to only create islands when edges are added but ultimately
    //makes the logic and iteration more confusing
    Island& island = graph.islands[islandIndex];
    island.propagation = propagation;
    island.nodes.push_back(nodeIndex);
  }

  void removeNode(Graph& graph, const NodeUserdata& data) {

  }
}