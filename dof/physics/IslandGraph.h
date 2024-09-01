#pragma once

#include "StableElementID.h"
#include "generics/FreeListContainer.h"

#include <iterator>

namespace IslandGraph {
  //Points at the stable id of the object in the pair
  using NodeUserdata = ElementRef;
  //Points at a pair entry in the spatial pairs table
  using EdgeUserdata = size_t;
  //Currently only all or nothing. None will show up in an island if connected to another propagating node.
  //Island traversal does not search through a non-propagating node
  using IslandPropagationMask = uint8_t;
  constexpr IslandPropagationMask PROPAGATE_ALL{ static_cast<IslandPropagationMask>(~0) };
  constexpr IslandPropagationMask PROPAGATE_NONE{ static_cast<IslandPropagationMask>(0) };
  using IslandIndex = uint16_t;

  struct IIslandUserdata {
    virtual ~IIslandUserdata() = default;
    virtual void clear() = 0;
  };

  struct IIslandUserdataFactory {
    virtual ~IIslandUserdataFactory() = default;
    virtual std::unique_ptr<IIslandUserdata> create() = 0;
  };

  static constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();
  static constexpr IslandIndex INVALID_ISLAND = std::numeric_limits<uint16_t>::max();
  struct Node {
    NodeUserdata data{};
    IslandIndex islandIndex{ INVALID_ISLAND };
    IslandPropagationMask propagation{};
    //Root of linked list of EdgeEntry
    uint32_t edges{ INVALID };
    uint32_t islandNext{ INVALID };
  };
  //This is the element of a linked list for all edges going out of a single node
  struct EdgeEntry {
    //The edge
    uint32_t edge{};
    //The next EdgeEntry in the list for this node
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
    uint32_t size() const {
      return nodeCount;
    }

    void clear() {
      Island temp;
      temp.userdata = std::move(userdata);
      *this = std::move(temp);
    }

    explicit operator bool() const {
      return nodes != INVALID;
    }

    uint32_t nodes{ INVALID };
    uint32_t edges{ INVALID };
    uint32_t nodeCount{};
    uint32_t edgeCount{};
    IslandIndex nextFreeIsland{};
    //Hacky solution to detect island changes for no-propagation nodes
    //Tracks all of them on the island since the node might belong to multiple islands
    //Hopefully there are few per island
    std::vector<uint32_t> noPropagateNodes;
    std::shared_ptr<IIslandUserdata> userdata;
  };
  constexpr static uint32_t FREE_INDEX = std::numeric_limits<uint32_t>::max() - 1;
}

namespace gnx {
  //Set the index type to uint32_t and point at one of the members to store a free index
  template<>
  struct FreeListTraits<IslandGraph::Node> {
    using ValueT = IslandGraph::Node;
    using IndexT = uint32_t;
    using Ops = MemberFreeOps<&IslandGraph::Node::islandNext, IslandGraph::FREE_INDEX>;
  };
  template<>
  struct FreeListTraits<IslandGraph::Edge> {
    using ValueT = IslandGraph::Edge;
    using IndexT = uint32_t;
    using Ops = MemberFreeOps<&IslandGraph::Edge::islandNext, IslandGraph::FREE_INDEX>;
  };
  template<>
  struct FreeListTraits<IslandGraph::EdgeEntry> {
    using ValueT = IslandGraph::EdgeEntry;
    using IndexT = uint32_t;
    using Ops = MemberFreeOps<&IslandGraph::EdgeEntry::edge, IslandGraph::FREE_INDEX>;
  };
  template<>
  struct FreeListTraits<IslandGraph::Island> {
    using ValueT = IslandGraph::Island;
    using IndexT = IslandGraph::IslandIndex;
    using Ops = MemberFreeOps<&IslandGraph::Island::nextFreeIsland, IslandGraph::INVALID_ISLAND>;
  };
}

namespace IslandGraph {
  struct Graph {
    struct ConstEdgeIterator {
      using iterator_category = std::random_access_iterator_tag;
      using value_type        = const EdgeUserdata;
      using difference_type   = size_t;
      using pointer           = const EdgeUserdata*;
      using reference         = const EdgeUserdata&;
      using iterator = ConstEdgeIterator;

      reference operator*() const {
        return graph->edges[edge].data;
      }

      pointer operator->() const {
        return &operator*();
      }

      iterator& operator++() {
        edge = graph->edges[edge].islandNext;
        return *this;
      }

      iterator operator++(int) {
        auto temp = *this;
        ++*this;
        return temp;
      }

      bool operator==(const iterator& _Right) const {
          return edge == _Right.edge;
      }

      bool operator!=(const iterator& _Right) const noexcept {
          return !(*this == _Right);
      }

      bool operator<(const iterator& _Right) const noexcept {
          return edge < _Right.edge;
      }

      bool operator>(const iterator& _Right) const noexcept {
          return _Right < *this;
      }

      bool operator<=(const iterator& _Right) const noexcept {
          return !(_Right < *this);
      }

      bool operator>=(const iterator& _Right) const noexcept {
        return !(*this < _Right);
      }

      const Graph* graph{};
      uint32_t edge{};
    };

    //Iterator over edges that these two objects share
    struct NodePairEdgeIterator {
      using iterator_category = std::forward_iterator_tag;
      using value_type        = const EdgeUserdata;
      using pointer           = const EdgeUserdata*;
      using reference         = const EdgeUserdata&;
      using iterator          = NodePairEdgeIterator;

      reference operator*() {
        return graph->edges[graph->edgeEntries[edgeEntry].edge].data;
      }

      pointer operator->() {
        return &operator*();
      }

      iterator& operator++() {
        //Keep going until the end or another edge pointing at B is found
        //These are the edges coming from A so they all match A
        while(true) {
          edgeEntry = graph->edgeEntries[edgeEntry].nextEntry;
          if(edgeEntry == INVALID || isMatchingEdge()) {
            break;
          }
        }
        return *this;
      }

      iterator operator++(int) {
        auto temp = *this;
        ++*this;
        return temp;
      }

      bool operator==(const iterator& _Right) const {
          return edgeEntry == _Right.edgeEntry;
      }

      bool operator!=(const iterator& _Right) const noexcept {
          return !(*this == _Right);
      }

      ConstEdgeIterator toEdgeIterator() const {
        return { graph, edgeEntry == INVALID ? INVALID : graph->edgeEntries[edgeEntry].edge };
      }

      bool isMatchingEdge() const {
        const EdgeEntry& ee = graph->edgeEntries[edgeEntry];
        const Edge& e = graph->edges[ee.edge];
        return e.nodeA == nodeB || e.nodeB == nodeB;
      }

      const Graph* graph{};
      //Current edge in this list of target node's entires
      uint32_t edgeEntry{};
      //The "other" node to find. edgeEntry is already all edges coming out of "nodeA"
      uint32_t nodeB{};
    };

    struct NodeIterator {
      using iterator_category = std::forward_iterator_tag;
      using value_type        = NodeUserdata;
      using difference_type   = size_t;
      using pointer           = NodeUserdata*;
      using reference         = NodeUserdata&;
      using iterator = NodeIterator;

      reference operator*() {
        return graph->nodes[node].data;
      }

      pointer operator->() {
        return &operator*();
      }

      iterator& operator++() {
        node = graph->nodes[node].islandNext;
        return *this;
      }

      iterator operator++(int) {
        auto temp = *this;
        ++*this;
        return temp;
      }

      bool operator==(const iterator& _Right) const {
          return node == _Right.node;
      }

      bool operator!=(const iterator& _Right) const noexcept {
          return !(*this == _Right);
      }

      bool operator<(const iterator& _Right) const noexcept {
          return node < _Right.node;
      }

      bool operator>(const iterator& _Right) const noexcept {
          return _Right < *this;
      }

      bool operator<=(const iterator& _Right) const noexcept {
          return !(_Right < *this);
      }

      bool operator>=(const iterator& _Right) const noexcept {
        return !(*this < _Right);
      }

      Graph* graph{};
      uint32_t node{};
    };

    struct ConstNodeIterator {
      using iterator_category = std::forward_iterator_tag;
      using value_type        = const NodeUserdata;
      using difference_type   = size_t;
      using pointer           = const NodeUserdata*;
      using reference         = const NodeUserdata&;
      using iterator = ConstNodeIterator;

      reference operator*() {
        return graph->nodes[node].data;
      }

      pointer operator->() {
        return &operator*();
      }

      iterator& operator++() {
        node = graph->nodes[node].islandNext;
        return *this;
      }

      iterator operator++(int) {
        auto temp = *this;
        ++*this;
        return temp;
      }

      bool operator==(const iterator& _Right) const {
          return node == _Right.node;
      }

      bool operator!=(const iterator& _Right) const noexcept {
          return !(*this == _Right);
      }

      bool operator<(const iterator& _Right) const noexcept {
          return node < _Right.node;
      }

      bool operator>(const iterator& _Right) const noexcept {
          return _Right < *this;
      }

      bool operator<=(const iterator& _Right) const noexcept {
          return !(_Right < *this);
      }

      bool operator>=(const iterator& _Right) const noexcept {
        return !(*this < _Right);
      }

      const Graph* graph{};
      uint32_t node{};
    };

    //Iterates over islands. Does include traversing the free list which are empty islands
    struct GraphIterator {
      using iterator_category = std::random_access_iterator_tag;
      using value_type        = Island;
      using difference_type   = size_t;
      using pointer           = Island*;
      using reference         = Island&;
      using iterator = GraphIterator;

      //Iterate over edges in an island
      ConstEdgeIterator beginEdges() {
        return { graph, graph->islands[islandIndex].edges };
      }

      ConstEdgeIterator endEdges() {
        return { graph, INVALID };
      }

      //Iterate over nodes in an island
      NodeIterator beginNodes() {
        return { graph, graph->islands[islandIndex].nodes };
      }

      NodeIterator endNodes() {
        return { graph, INVALID };
      }

      reference operator*() {
        return graph->islands[islandIndex];
      }

      pointer operator->() {
        return &graph->islands[islandIndex];
      }

      GraphIterator& operator++() {
        ++islandIndex;
        return *this;
      }

      GraphIterator operator++(int) {
        auto temp = *this;
        ++*this;
        return temp;
      }

      GraphIterator& operator--() {
        --islandIndex;
        return *this;
      }

      GraphIterator operator--(int) {
        auto temp = *this;
        --*this;
        return temp;
      }

      GraphIterator& operator+=(const difference_type _Off) {
          islandIndex += _Off;
          return *this;
      }

      GraphIterator operator+(const difference_type _Off) const {
          auto temp = *this;
          temp += _Off;
          return temp;
      }

      GraphIterator friend operator+(const difference_type _Off, GraphIterator _Next) {
          _Next += _Off;
          return _Next;
      }

      GraphIterator& operator-=(const difference_type _Off) {
          return *this += -_Off;
      }

      GraphIterator operator-(const difference_type _Off) const {
          auto temp = *this;
          temp -= _Off;
          return temp;
      }

      difference_type operator-(const GraphIterator& _Right) const {
          return islandIndex - _Right.islandIndex;
      }

      reference operator[](const difference_type _Off) const {
          return *(*this + _Off);
      }

      bool operator==(const GraphIterator& _Right) const {
          return islandIndex == _Right.islandIndex;
      }

      bool operator!=(const GraphIterator& _Right) const noexcept {
          return !(*this == _Right);
      }

      bool operator<(const GraphIterator& _Right) const noexcept {
          return islandIndex < _Right.islandIndex;
      }

      bool operator>(const GraphIterator& _Right) const noexcept {
          return _Right < *this;
      }

      bool operator<=(const GraphIterator& _Right) const noexcept {
          return !(_Right < *this);
      }

      bool operator>=(const GraphIterator& _Right) const noexcept {
        return !(*this < _Right);
      }

      Graph* graph{};
      size_t islandIndex{};
    };

    //Iterate over each island
    GraphIterator begin() {
      return { this, 0 };
    }

    GraphIterator end() {
      return { this, islands.values.size() };
    }

    //EdgeIterator findEdge(const NodeUserdata& a, const NodeUserdata& b);
    NodeIterator findNode(const NodeUserdata& node);
    NodePairEdgeIterator findEdge(const NodeUserdata& a, const NodeUserdata& b) const;
    ConstNodeIterator findNode(const NodeUserdata& node) const;

    NodePairEdgeIterator edgesEnd() const {
      return { this, INVALID, INVALID };
    }

    ConstEdgeIterator cEdgesEnd() const {
      return { this, INVALID };
    }

    NodeIterator nodesEnd() {
      return { this, INVALID };
    }

    ConstNodeIterator nodesEnd() const {
      return { this, INVALID };
    }

    gnx::VectorFreeList<Node> nodes;
    gnx::VectorFreeList<Edge> edges;
    gnx::VectorFreeList<EdgeEntry> edgeEntries;
    std::unordered_map<NodeUserdata, NodeMappings> nodeMappings;
    gnx::VectorFreeList<Island> islands;
    //Assuming bitset optimization
    //Islands that have changed for internal tracking of work to do in rebuildIslands
    std::vector<bool> changedIslands;
    std::vector<bool> visitedNodes, visitedEdges;
    //Information about which islands have changed nodes or edges for external use after rebuildIslands
    std::vector<bool> publishedIslandNodesChanged, publishedIslandEdgesChanged;
    std::vector<uint32_t> newNodes;
    std::vector<uint32_t> scratchBuffer, scratchIslands;
    //Unique ownership usually but shared easy debugging allowing copying
    std::shared_ptr<IIslandUserdataFactory> userdataFactory;
  };

  EdgeUserdata addUnmappedEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b);
  void addEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b, const EdgeUserdata& edge);
  void removeEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b);
  void removeEdge(Graph& graph, const Graph::ConstEdgeIterator& it);
  void addNode(Graph& graph, const NodeUserdata& data, IslandPropagationMask propagation = PROPAGATE_ALL);
  void removeNode(Graph& graph, const NodeUserdata& data);
  void notifyNodeChanged(Graph& graph, Graph::NodeIterator it);
  void setPropagation(Graph& graph, Graph::NodeIterator it, IslandPropagationMask propagation);

  void rebuildIslands(Graph& graph);

  namespace Debug {
    //Validate that all expected nodes and edges in the graph are traversible via islands
    //Valid result is expected after rebuildIslands and until the next modification to the graph
    bool validateIslands(Graph& graph);
    void rebuildAndValidateIslands(Graph& graph);
  }
}