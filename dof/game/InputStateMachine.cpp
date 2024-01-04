#include "Precompile.h"
#include "InputStateMachine.h"
#include "GameMath.h"
#include "generics/LinkedList.h"

namespace gnx::LinkedList {
  struct NodeEdgeListTraits {
    static Input::NodeIndex& getIndex(Input::Node& node) { return node.edges; }
    static const Input::NodeIndex& getIndex(const Input::Node& node) { return node.edges; }
  };

  struct NodeSubnodeListTraits {
    static Input::NodeIndex& getIndex(Input::Node& node) { return node.subnodes; }
    static const Input::NodeIndex& getIndex(const Input::Node& node) { return node.subnodes; }
  };

  template<>
  struct Traits<Input::Edge> {
    static Input::EdgeIndex& getIndex(Input::Edge& edge) { return edge.edges; }
    static const Input::EdgeIndex& getIndex(const Input::Edge& edge) { return edge.edges; }
  };
};

namespace Input {
  constexpr float UNSET1D = std::numeric_limits<float>::max();
  constexpr glm::vec2 UNSET2D{ UNSET1D, UNSET1D };
  constexpr EdgeTraverser NOOP_TRAVERSER{};

  void addToActiveList(NodeIndex i, std::vector<NodeIndex>& activeNodes) {
    activeNodes.push_back(i);
  }

  void removeFromActiveList(std::vector<NodeIndex>::iterator it, std::vector<NodeIndex>& activeNodes) {
    *it = activeNodes.back();
    activeNodes.pop_back();
  }

  StateMachineBuilder::StateMachineBuilder() {
    //Add root
    nodes.push_back({});
  }

  StateMachine::StateMachine(StateMachineBuilder&& builder, InputMapper&& m)
    : mapper{ std::move(m) }
  {
    builder.nodes[ROOT_NODE].isActive = true;
    addToActiveList(ROOT_NODE, activeNodes);
    mapper.bind(builder);
    nodes = std::move(builder.nodes);
    edges = std::move(builder.edges);
  }

  NodeIndex StateMachineBuilder::addNode(const Node& node) {
    NodeIndex result = static_cast<NodeIndex>(nodes.size());
    nodes.push_back(node);
    return result;
  }

  EdgeIndex StateMachineBuilder::addEdge(NodeIndex from, NodeIndex to, const Edge& edge) {
    return addEdge({ from, to, edge });
  }

  EdgeIndex StateMachineBuilder::addEdge(const EdgeData& data) {
    EdgeIndex result = static_cast<EdgeIndex>(edges.size());
    edges.push_back(data.edge);
    Edge& e = edges.back();
    e.to = data.to;
    gnx::LinkedList::insertAfter(nodes[data.from], e, result, gnx::LinkedList::NodeEdgeListTraits{});

    return result;
  }

  struct CanTraverse {
    bool operator()(const Edge::Empty) { return true; }
    bool operator()(const Edge::KeyUp&) { return true; }
    bool operator()(const Edge::KeyDown&) { return true; }
    bool operator()(const Edge::Timeout& e) {
      return e.timeoutAfter <= std::get<Edge::Timeout>(traverser.data).timeoutAfter;
    }
    bool operator()(const Edge::Delta1D& e) {
      auto& traverseData = std::get<Edge::Delta1D>(traverser.data);
      if(traverseData.minDelta == UNSET1D) {
        //Relative information is missing, fill it in from the absolute data
        traverseData.minDelta = traverseData.maxDelta - machine.getAbsoluteAxis1D(edge.to);
      }
      else if(traverseData.maxDelta == UNSET1D) {
        //Absolute information is missing, fill it in from the relative data
        traverseData.maxDelta = traverseData.minDelta + machine.getAbsoluteAxis1D(edge.to);
      }

      if(edge.isAbsolute) {
        return Math::between(traverseData.maxDelta, e.minDelta, e.maxDelta);
      }

      return Math::between(traverseData.minDelta, e.minDelta, e.maxDelta);
    }
    bool operator()(const Edge::Delta2D& e) {
      auto& traverseData = std::get<Edge::Delta2D>(traverser.data);
      if(traverseData.minDelta == UNSET2D) {
        //Relative information is missing, fill it in from the absolute data
        traverseData.minDelta = traverseData.maxDelta - machine.getAbsoluteAxis2D(edge.to);
      }
      else if(traverseData.maxDelta == UNSET2D) {
        //Absolute information is missing, fill it in from the relative data
        traverseData.maxDelta = traverseData.minDelta + machine.getAbsoluteAxis2D(edge.to);
      }

      if(edge.isAbsolute) {
        return Math::between(traverseData.maxDelta, e.minDelta, e.maxDelta);
      }

      return Math::between(traverseData.minDelta, e.minDelta, e.maxDelta);
    }

    bool shouldTraverseEdge() {
      //Unconditional edge can always be traversed
      if(std::get_if<Edge::Empty>(&edge.data)) {
        return true;
      }
      //Key needs to match or be unset, then check the variants
      if((edge.key == INVALID_KEY || edge.key == traverser.key) && edge.data.index() == traverser.data.index()) {
        //Variants match, see if the conditions within the variant are met
        return std::visit(*this, edge.data);
      }
      return false;
    }

    EdgeTraverser& traverser;
    const StateMachine& machine;
    const Edge& edge;
  };

  void enterNode(Node& node, const EdgeTraverser& traverser) {
    //Set the node itself as active. It may be a subnode
    //The subnode should be active while the root node gets added as the active index
    node.isActive = true;
    if(auto* node1 = std::get_if<Node::Axis1D>(&node.data)) {
      if(const float* data = traverser.tryGetAxis1DAbsolute()) {
        node1->absolute = *data;
      }
    }
    else if(auto* node2 = std::get_if<Node::Axis2D>(&node.data)) {
      if(const glm::vec2* data = traverser.tryGetAxis2DAbsolute()) {
        node2->absolute = *data;
      }
    }
  }

  void exitNode(NodeIndex root, std::vector<Node>& nodes) {
    Node& node = nodes[root];
    node.isActive = false;
    node.timeActive = {};
  }

  //Subnode is the distance in the linked list to traverse
  NodeIndex getSubnode(NodeIndex root, NodeIndex subnodeIndex, std::vector<Node>& nodes) {
    while(subnodeIndex-- > 0) {
      root = nodes[root].subnodes;
    }
    return root;
  }

  NodeIndex getNodeOrSubnode(NodeIndex root, NodeIndex subnodeIndex, std::vector<Node>& nodes) {
    return subnodeIndex == INVALID_NODE ? root : getSubnode(root, subnodeIndex, nodes);
  }

  void StateMachine::traverse(const EdgeTraverser& et) {
    //Early out for no-op case
    if(std::get_if<Edge::Empty>(&et.data)) {
      return;
    }

    EdgeTraverser traverser{ et };
    Timespan elapsed{};
    if(const auto* t = std::get_if<Edge::Timeout>(&traverser.data)){ 
      elapsed = t->timeoutAfter;
    }
    for(size_t i = 0; i < activeNodes.size();) {
      NodeIndex active = activeNodes[i];
      Node& node = nodes[active];
      node.timeActive += elapsed;
      bool removedActiveNode = false;
      bool eventConsumed = false;
      //Try all edges going out of active nodes
      gnx::LinkedList::foreach(edges, node.edges, [&](Edge& edge) {
        CanTraverse canTraverse{ traverser, *this, edge };
        if(canTraverse.shouldTraverseEdge()) {
          const bool isKeyMatch = edge.key == traverser.key;
          //ROOT special case means exit the machine, so no node entry logic is needed
          if(edge.to != ROOT_NODE) {
            Node& root = nodes[edge.to];
            //If the node is already active it's fine to update the data on the node but it shouldn't be
            //added again into the active list nor trigger events
            const bool wasAlreadyActive = isNodeActive(edge.to);
            //If there is a subnode the updated data needs to be stored there
            enterNode(nodes[getNodeOrSubnode(edge.to, isKeyMatch ? traverser.toSubnode : INVALID_NODE, nodes)], traverser);
            if(!wasAlreadyActive) {
              //Regardless of subnodes only the root is ever considered active, so add that to the list
              addToActiveList(edge.to, activeNodes);
              //TODO: does this work in the case of multiple subnodes triggering an input to an active node?
              assert(root.activeSubnode == INVALID_NODE);
              root.activeSubnode = isKeyMatch ? traverser.toSubnode : INVALID_NODE;

              if(root.event != INVALID_EVENT) {
                //Emit the event of the destination node with the time spent in the source node
                publishedEvents.push_back({ root.event, edge.to, node.timeActive, traverser });
              }
            }
          }
          //Either this is an exit condition or the state moved from one edge to another, deactivate source node
          //It should only remain if the execution is forking meaning both the source and destination are now active
          if(active != ROOT_NODE && (edge.to == ROOT_NODE || !edge.fork)) {
            //Exit the node or subnode. Once all subnodes are inactive the root node can be removed from the active list
            exitNode(getNodeOrSubnode(active, node.activeSubnode, nodes), nodes);

            if(!isNodeActive(active)) {
              exitNode(active, nodes);
              node.isActive = false;
              removeFromActiveList(activeNodes.begin() + i, activeNodes);
              node.activeSubnode = INVALID_NODE;
              removedActiveNode = true;
              if(edge.consumeEvent) {
                eventConsumed = true;
              }
              return false;
            }
          }
          if(edge.consumeEvent) {
            eventConsumed = true;
            return false;
          }
        }
        return true;
      });

      if(eventConsumed) {
        return;
      }
      if(!removedActiveNode) {
        ++i;
      }
    }
  }

  bool StateMachine::isNodeActive(NodeIndex nodeIndex) const {
    bool isActive = false;
    gnx::LinkedList::foreach(nodes, nodeIndex, [&isActive](const Node& node) {
      isActive = isActive || node.isActive;
      return !isActive;
    }, gnx::LinkedList::NodeSubnodeListTraits{});
    return isActive;
  }

  float tryGet1D(const Node::Variant& n) {
    const auto* value = std::get_if<Node::Axis1D>(&n);
    return value ? value->absolute : 0.0f;
  }

  glm::vec2 tryGet2D(const Node::Variant& n) {
    const auto* value = std::get_if<Node::Axis2D>(&n);
    return value ? value->absolute : glm::vec2{ 0, 0 };
  }

  float StateMachine::getAbsoluteAxis1D(NodeIndex nodeIndex) const {
    float result{};
    gnx::LinkedList::foreach(nodes, nodeIndex, [&result](const Node& node) {
      result += tryGet1D(node.data);
    }, gnx::LinkedList::NodeSubnodeListTraits{});
    return result;
  }

  glm::vec2 StateMachine::getAbsoluteAxis2D(NodeIndex nodeIndex) const {
    glm::vec2 result{ 0 };
    gnx::LinkedList::foreach(nodes, nodeIndex, [&result](const Node& node) {
      result += tryGet2D(node.data);
    }, gnx::LinkedList::NodeSubnodeListTraits{});
    return result;
  }

  const std::vector<Event>& StateMachine::readEvents() const {
    return publishedEvents;
  }

  void StateMachine::clearEvents() {
    publishedEvents.clear();
  }

  const InputMapper& StateMachine::getMapper() {
    return mapper;
  }

  void InputMapper::bind(StateMachineBuilder& machine) {
    for(auto&& [platformKey, value] : mappings) {
      const KeyMapID keyMapKey = value.traverser.key;
      //If there are redundant nodes, subnodes need to be inserted
      if(auto reverse = reverseMappings.find(keyMapKey); reverse != reverseMappings.end() && reverse->second.subnodeCount > 0) {
        reverse->second.currentSubnode++;
        //All edges matching the KeyMapID need to gain a phantom node to hold the redundant state
        //which is within the linked list of subnodes
        const size_t edgesEnd = machine.edges.size();
        for(size_t e = 0; e < edgesEnd; ++e) {
          if(machine.edges[e].key == keyMapKey) {
            //Add associated subnode for incoming edges to the node
            {
              Node& root = machine.nodes[machine.edges[e].to];
              Node copy = root;
              copy.edges = INVALID_EDGE;
              const NodeIndex subnodeIndex = static_cast<NodeIndex>(machine.nodes.size());
              gnx::LinkedList::insertAfter(root, copy, subnodeIndex, gnx::LinkedList::NodeSubnodeListTraits{}, gnx::LinkedList::NodeSubnodeListTraits{});
              machine.nodes.push_back(copy);
              value.traverser.toSubnode = reverse->second.currentSubnode;
            }
            //Add associated subnode for outgoing edges from the node
            {
              Node& root = machine.nodes[machine.edges[e].from];
              Node copy = root;
              copy.edges = INVALID_EDGE;
              const NodeIndex subnodeIndex = static_cast<NodeIndex>(machine.nodes.size());
              gnx::LinkedList::insertAfter(root, copy, subnodeIndex, gnx::LinkedList::NodeSubnodeListTraits{}, gnx::LinkedList::NodeSubnodeListTraits{});
              machine.nodes.push_back(copy);
              value.traverser.fromSubnode = reverse->second.currentSubnode;
            }
          }
        }
      }
      //If there are no redundant nodes, the single node by itself is all that needs to be found
    }
  }

  template<class EdgeT>
  void InputMapper::addMapping(PlatformInputID src, KeyMapID dst, EdgeT&& edge) {
    auto [platformMapping, isNewPlatform] = mappings.emplace(src, Mapping{});
    auto [reverseMapping, isNewReverse] = reverseMappings.emplace(dst, ReverseMapping{});
    assert(isNewPlatform);
    EdgeTraverser& traverser = platformMapping->second.traverser;

    //Associate the platform input with the key, node indices will be filled in later
    traverser.data = std::move(edge);
    traverser.key = dst;
    //If there was already a mapping to this key, increment the counter for later
    if(!isNewReverse) {
      reverseMapping->second.subnodeCount++;
    }
  }

  void InputMapper::addKeyMapping(PlatformInputID src, KeyMapID dst) {
    addMapping(src, dst, Edge::KeyDown{});
  }

  void InputMapper::addAxis1DMapping(PlatformInputID src, KeyMapID dst) {
    addMapping(src, dst, Edge::Delta1D{});
  }

  void InputMapper::addAxis2DMapping(PlatformInputID src, KeyMapID dst) {
    addMapping(src, dst, Edge::Delta2D{});
  }

  void InputMapper::addKeyAs1DRelativeMapping(PlatformInputID src, KeyMapID dst, float amount) {
    Edge::Delta1D e;
    e.minDelta = e.maxDelta = amount;
    addMapping(src, dst, std::move(e));
  }

  void InputMapper::addKeyAs2DRelativeMapping(PlatformInputID src, KeyMapID dst, const glm::vec2& amount) {
    Edge::Delta2D e;
    e.minDelta = e.maxDelta = amount;
    addMapping(src, dst, std::move(e));
  }

  EdgeTraverser InputMapper::onPassthroughKeyDown(KeyMapID key) const {
    EdgeTraverser result;
    result.key = key;
    result.data.emplace<Edge::KeyDown>();
    return result;
  }

  //Keys are stored as key down events so this case can return directly
  EdgeTraverser InputMapper::onKeyDown(PlatformInputID key) const {
    auto it = mappings.find(key);
    return it != mappings.end() ? it->second.traverser : NOOP_TRAVERSER;
  }

  template<class T>
  void undoDirectionKey(T& data) {
    auto& absolute = data.maxDelta;
    auto& relative = data.minDelta;
    absolute = {};
    relative = -relative;
  }

  //Keys are stored as key down so this needs to flip it to key up before returning
  EdgeTraverser InputMapper::onKeyUp(PlatformInputID key) const {
    if(auto it = mappings.find(key); it != mappings.end()) {
      EdgeTraverser result = it->second.traverser;
      if(std::get_if<Edge::KeyDown>(&result.data)) {
        result.data.emplace<Edge::KeyUp>();
      }
      //Flip directions since the key is being released
      else if(Edge::Delta1D* d1 = std::get_if<Edge::Delta1D>(&result.data)) {
        undoDirectionKey(*d1);
      }
      else if(Edge::Delta2D* d2 = std::get_if<Edge::Delta2D>(&result.data)) {
        undoDirectionKey(*d2);
      }
      return result;
    }
    return NOOP_TRAVERSER;
  }

  EdgeTraverser InputMapper::onAxis1DRelative(PlatformInputID axis, float relative) const {
    if(auto it = mappings.find(axis); it != mappings.end()) {
      EdgeTraverser result = it->second.traverser;
      if(auto* data = std::get_if<Edge::Delta1D>(&result.data)) {
        data->minDelta = relative;
        data->maxDelta = UNSET1D;
        return result;
      }
    }
    return NOOP_TRAVERSER;
  }

  EdgeTraverser InputMapper::onAxis1DAbsolute(PlatformInputID axis, float absolute) const {
    if(auto it = mappings.find(axis); it != mappings.end()) {
      EdgeTraverser result = it->second.traverser;
      if(auto* data = std::get_if<Edge::Delta1D>(&result.data)) {
        data->minDelta = UNSET1D;
        data->maxDelta = absolute;
        return result;
      }
    }
    return NOOP_TRAVERSER;
  }

  EdgeTraverser InputMapper::onAxis2DRelative(PlatformInputID axis, const glm::vec2& relative) const {
    if(auto it = mappings.find(axis); it != mappings.end()) {
      EdgeTraverser result = it->second.traverser;
      if(auto* data = std::get_if<Edge::Delta2D>(&result.data)) {
        data->minDelta = relative;
        data->maxDelta = UNSET2D;
        return result;
      }
    }
    return NOOP_TRAVERSER;
  }

  EdgeTraverser InputMapper::onAxis2DAbsolute(PlatformInputID axis, const glm::vec2& absolute) const {
    if(auto it = mappings.find(axis); it != mappings.end()) {
      EdgeTraverser result = it->second.traverser;
      if(auto* data = std::get_if<Edge::Delta2D>(&result.data)) {
        data->minDelta = UNSET2D;
        data->maxDelta = absolute;
        return result;
      }
    }
    return NOOP_TRAVERSER;
  }

  EdgeTraverser InputMapper::onTick(Timespan timeElapsed) const {
    EdgeTraverser result;
    result.data.emplace<Edge::Timeout>(timeElapsed);
    return result;
  }
};