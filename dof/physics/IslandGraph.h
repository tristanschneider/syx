#pragma once

#include "StableElementID.h"
#include "generics/FreeListContainer.h"

#include <iterator>

namespace IslandGraph {
  //Points at the object in the pair
  using NodeUserdata = StableElementID;
  //Points at a pair entry in the spatial pairs table
  using EdgeUserdata = StableElementID;
  //When an edge is added between two nodes, their islands will be joined if the bitwise or of these is nonzero
  //Primary use case is spatial queries or static colliders where what they are close to should still be known
  //but they don't affect constraint solving so don't need to contribute to the island
  using IslandPropagationMask = uint8_t;
  //TODO: should this have a bits for these cases?
  //Add to my island
  //Merge islands
  //An edge from default to static object would want to add to default and not merge
  //An edge from default to default would want to merge
  //An edge from static to static should show up somewhere but ideally not duplicated
  constexpr IslandPropagationMask PROPAGATE_ALL{ static_cast<IslandPropagationMask>(~0) };
  constexpr IslandPropagationMask PROPAGATE_NONE{ static_cast<IslandPropagationMask>(0) };

  /*
  struct Node {
    void markAsFree() {
      islandIndex = std::numeric_limits<uint32_t>::max();
    }

    bool isFree() const {
      return islandIndex == std::numeric_limits<uint32_t>::max();
    }

    NodeUserdata data{};
    uint32_t islandIndex{};
  };

  struct Edge {
    void markAsFree() {
      edge.indices.first = std::numeric_limits<uint32_t>::max();
    }

    bool isFree() const {
      return edge.indices.first == std::numeric_limits<uint32_t>::max();
    }

    union {
      std::pair<uint32_t, uint32_t> indices;
      uint64_t key;
    } edge;
    EdgeUserdata data{};
  };

  struct Island {
    void markAsFree() {
      nodes.clear();
      edges.clear();
    }

    bool isFree() const {
      return nodes.empty();
    }

    std::vector<size_t> nodes;
    std::vector<size_t> edges;
    IslandPropagationMask propagation{};
    // Can be used during integration to skip nodes
    // Can be used during constraint solving to skip the island
    // Can be used during narrowphase to skip the island
    // Reset when adding an edge to an island
    // Must be reset by user when an impulse is applied to an object in a sleeping island
    // Is set by the user when they find all nodes in the island aren't moving
    bool isSleeping{};
  };
  struct Graph {
    struct EdgeIterator {
      using iterator_category = std::random_access_iterator_tag;
      using value_type        = EdgeUserdata;
      using difference_type   = size_t;
      using pointer           = EdgeUserdata*;
      using reference         = EdgeUserdata&;
      using iterator = EdgeIterator;

      reference operator*() {
        const size_t index = graph->islands[islandIndex].edges[edgeIndex];
        return graph->edges[index].data;
      }

      pointer operator->() {
        return &operator*();
      }

      iterator& operator++() {
        ++edgeIndex;
        return *this;
      }

      iterator operator++(int) {
        auto temp = *this;
        ++*this;
        return temp;
      }

      iterator& operator--() {
        --edgeIndex;
        return *this;
      }

      iterator operator--(int) {
        auto temp = *this;
        --*this;
        return temp;
      }

      bool operator==(const iterator& _Right) const {
          return edgeIndex == _Right.edgeIndex;
      }

      bool operator!=(const iterator& _Right) const noexcept {
          return !(*this == _Right);
      }

      bool operator<(const iterator& _Right) const noexcept {
          return edgeIndex < _Right.edgeIndex;
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
      size_t islandIndex{};
      size_t edgeIndex{};
    };

    struct NodeIterator {
      using iterator_category = std::random_access_iterator_tag;
      using value_type        = NodeUserdata;
      using difference_type   = size_t;
      using pointer           = NodeUserdata*;
      using reference         = NodeUserdata&;
      using iterator = NodeIterator;

      reference operator*() {
        const size_t index = graph->islands[islandIndex].nodes[nodeIndex];
        return graph->nodes[index].data;
      }

      pointer operator->() {
        return &operator*();
      }

      iterator& operator++() {
        ++nodeIndex;
        return *this;
      }

      iterator operator++(int) {
        auto temp = *this;
        ++*this;
        return temp;
      }

      iterator& operator--() {
        --nodeIndex;
        return *this;
      }

      iterator operator--(int) {
        auto temp = *this;
        --*this;
        return temp;
      }

      bool operator==(const iterator& _Right) const {
          return nodeIndex == _Right.nodeIndex;
      }

      bool operator!=(const iterator& _Right) const noexcept {
          return !(*this == _Right);
      }

      bool operator<(const iterator& _Right) const noexcept {
          return nodeIndex < _Right.nodeIndex;
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
      size_t islandIndex{};
      size_t nodeIndex{};
    };

    struct GraphIterator {
      using iterator_category = std::random_access_iterator_tag;
      using value_type        = Island;
      using difference_type   = size_t;
      using pointer           = Island*;
      using reference         = Island&;

      //Iterate over edges in an island
      EdgeIterator beginEdges() {
        return { graph, islandIndex, 0 };
      }

      EdgeIterator endEdges() {
        return { graph, islandIndex, graph->islands[islandIndex].edges.size() };
      }

      //Iterate over nodes in an island
      NodeIterator beginNodes() {
        return { graph, islandIndex, 0 };
      }

      NodeIterator endNodes() {
        return { graph, islandIndex, graph->islands[islandIndex].nodes.size() };
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
      //TODO: special index zero for static island?
      return { this, 0 };
    }

    GraphIterator end() {
      return { this, islands.size() };
    }

    EdgeIterator findEdge(const NodeUserdata& a, const NodeUserdata& b);

    EdgeIterator edgesEnd() {
      return { this, islands.size(), 0 };
    }

    struct NodeMappings {
      uint32_t island{};
      uint32_t node{};
    };

    std::vector<Island> islands;
    std::vector<uint32_t> islandFreeList;
    std::vector<Node> nodes;
    std::vector<uint32_t> nodeFreeList;
    std::vector<Edge> edges;
    std::vector<uint32_t> edgeFreeList;
    std::unordered_map<NodeUserdata, NodeMappings> nodeMappings;
  };

  void addEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b, const EdgeUserdata& edge);
  void removeEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b);
  void addNode(Graph& graph, const NodeUserdata& data, IslandPropagationMask propagation = PROPAGATE_ALL);
  void removeNode(Graph& graph, const NodeUserdata& data);
  */
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

    uint32_t nodes{ INVALID };
    uint32_t edges{ INVALID };
    uint32_t nodeCount{};
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
}

namespace IslandGraph {
  struct Graph {

    struct EdgeIterator {
      using iterator_category = std::random_access_iterator_tag;
      using value_type        = EdgeUserdata;
      using difference_type   = size_t;
      using pointer           = EdgeUserdata*;
      using reference         = EdgeUserdata&;
      using iterator = EdgeIterator;

      reference operator*() {
        return graph->edges[edge].data;
      }

      pointer operator->() {
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

      Graph* graph{};
      uint32_t edge{};
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

    struct GraphIterator {
      using iterator_category = std::random_access_iterator_tag;
      using value_type        = Island;
      using difference_type   = size_t;
      using pointer           = Island*;
      using reference         = Island&;
      using iterator = GraphIterator;

      //Iterate over edges in an island
      EdgeIterator beginEdges() {
        return { graph, graph->islands[islandIndex].edges };
      }

      EdgeIterator endEdges() {
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
      return { this, islands.size() };
    }

    EdgeIterator findEdge(const NodeUserdata& a, const NodeUserdata& b);

    EdgeIterator edgesEnd() {
      return { this, INVALID };
    }

    gnx::VectorFreeList<Node> nodes;
    gnx::VectorFreeList<Edge> edges;
    gnx::VectorFreeList<EdgeEntry> edgeEntries;
    std::unordered_map<NodeUserdata, NodeMappings> nodeMappings;
    std::vector<Island> islands;
    std::vector<uint32_t> scratchBuffer;
  };

  void addEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b, const EdgeUserdata& edge);
  void removeEdge(Graph& graph, const NodeUserdata& a, const NodeUserdata& b);
  void addNode(Graph& graph, const NodeUserdata& data, IslandPropagationMask propagation = PROPAGATE_ALL);
  void removeNode(Graph& graph, const NodeUserdata& data);
  void rebuildIslands(Graph& graph);
}