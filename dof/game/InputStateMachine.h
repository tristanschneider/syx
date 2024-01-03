#pragma once

#include "glm/vec2.hpp"

namespace Input {
  using PlatformInputID = uint32_t;
  using EventID = uint32_t;
  using KeyMapID = uint32_t;
  using NodeIndex = uint32_t;
  using EdgeIndex = uint32_t;
  using Timespan = uint32_t;

  constexpr NodeIndex INVALID_NODE = std::numeric_limits<NodeIndex>::max();
  constexpr EdgeIndex INVALID_EDGE = std::numeric_limits<EdgeIndex>::max();
  constexpr EventID INVALID_EVENT = std::numeric_limits<EventID>::max();
  constexpr KeyMapID INVALID_KEY = std::numeric_limits<KeyMapID>::max();

  //Nodes have an overloaded meaning depending on if they are a subnode or not
  //A node is the logical information about a particular input state in the graph
  //For plain states this may only be an active bool, for axes it's the current axis value
  //Subnodes are additional data sources for that stored information in the case of redundant mappings
  //The logical node in the graph is queried as if it was all on the node but it may require searching
  //through multiple subnodes to accumulate all of the individual inputs to arrive at the final result
  struct Node {
    struct Empty {};
    struct Button {};
    struct Axis1D {
      float absolute{};
    };
    struct Axis2D {
      glm::vec2 absolute{ 0.0f };
    };
    using Variant = std::variant<Empty, Button, Axis1D, Axis2D>;
    Variant data;
    //Intrusive linked list of edges
    EdgeIndex edges{ INVALID_EDGE };
    //For nodes that have multiple mappings pointing at them, the root mapping will
    //be the beginning of an intrusive linked list of other mappings that contribute to the primary
    //Determining the node state requires resolving all of the subnodes.
    //For example, if a direction key and a thumbstick direction is held, both contribute to the direction
    NodeIndex subnodes{ INVALID_NODE };
    EventID event{ INVALID_EVENT };
    Timespan timeActive{};
    //The active state on a node means if this node itself was directly traversed
    //In the case of subnodes this means it is possible for a node's index to be in the active
    //nodes list but for the node itself not to be active as long as one of its subnodes is
    bool isActive{};
  };

  //Edges are the mechanism to move between states in the graph. They contain the criteria
  //for allowing traversal while not themselves knowing of the particular current input state
  struct Edge {
    struct Empty {};
    struct KeyDown {
    };
    struct KeyUp {
    };
    struct Timeout {
      Timespan timeoutAfter{};
    };
    struct Delta1D {
      float minDelta{};
      float maxDelta{};
    };
    struct Delta2D {
      glm::vec2 minDelta{ 0 };
      glm::vec2 maxDelta{ 0 };
    };

    operator bool() const {
      return std::get_if<Empty>(&data) == nullptr;
    }

    using Variant = std::variant<Empty, KeyDown, KeyUp, Timeout, Delta1D, Delta2D>;
    KeyMapID key{ INVALID_KEY };
    Variant data;
    //Stop further traversal immediately if this edge is traversed
    bool consumeEvent{};
    //When being traversed, this will leave the source node active rather than moving
    //exclusively to the destination node
    bool fork{};
    NodeIndex from{ INVALID_NODE };
    NodeIndex to{ INVALID_NODE };
    //Intrusive linked list of other edges attached to the node
    EdgeIndex edges{ INVALID_EDGE };
  };

  //This is the input to the state machine that is mached against edges to see if they are applicable
  //The variant is the same since it needs to match against each possible variant but the interpretation
  //of what is stored in an edge variant vs traverser variant is a bit different
  struct EdgeTraverser {
    float getAxis1DDelta() const {
      return std::get<Edge::Delta1D>(data).minDelta;
    }

    float getAxis1DAbsolute() const {
      return std::get<Edge::Delta1D>(data).maxDelta;
    }

    glm::vec2 getAxis2DDelta() const {
      return std::get<Edge::Delta2D>(data).minDelta;
    }

    glm::vec2 getAxis2DAbsolute() const {
      return std::get<Edge::Delta2D>(data).maxDelta;
    }

    KeyMapID key{};
    //If populated, then this is a simulated edge meaning that the subnode holds the
    //destination state rather than the actual edge location. It is an offset in the
    //node's linked list of subnodes, not the direct index of the subnode.
    NodeIndex toSubnode{ INVALID_NODE };
    NodeIndex fromSubnode{ INVALID_NODE };
    Edge::Variant data;
  };

  struct Event {
    EventID id{};
    //Time spent in the previous node before traversing the edge that triggered this event
    Timespan timeInNode{};
    //The traverser that triggered this event. Can be observed to get input delta
    //Absolute values can be obtained by looking at the nodes themselves
    EdgeTraverser traverser;
  };

  struct EdgeData {
    NodeIndex from{};
    NodeIndex to{};
    Edge edge;
  };

  struct EdgeBuilder {
    EdgeBuilder& from(Input::KeyMapID id) {
      data.edge.from = id;
      data.from = id;
      return *this;
    }

    EdgeBuilder& to(Input::KeyMapID id) {
      data.to = id;
      return *this;
    }

    EdgeBuilder& keyDown(Input::KeyMapID id) {
      data.edge.key = id;
      data.edge.data.emplace<Input::Edge::KeyDown>();
      return *this;
    }

    EdgeBuilder& keyUp(Input::KeyMapID id) {
      data.edge.key = id;
      data.edge.data.emplace<Input::Edge::KeyUp>();
      return *this;
    }

    EdgeBuilder& unconditional() {
      data.edge.data.emplace<Input::Edge::Empty>();
      return *this;
    }

    EdgeBuilder& delta1D(Input::KeyMapID id, float min, float max) {
      data.edge.key = id;
      data.edge.data.emplace<Input::Edge::Delta1D>(min, max);
      return *this;
    }

    EdgeBuilder& anyDelta1D(Input::KeyMapID id) {
      return delta1D(id, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max());
    }

    EdgeBuilder& delta2D(Input::KeyMapID id, const glm::vec2& min, const glm::vec2& max) {
      data.edge.key = id;
      data.edge.data.emplace<Input::Edge::Delta2D>(min, max);
      return *this;
    }

    EdgeBuilder& anyDelta2D(Input::KeyMapID id) {
      return delta2D(id, glm::vec2{ std::numeric_limits<float>::lowest() }, glm::vec2{ std::numeric_limits<float>::max() });
    }

    EdgeBuilder& forkState() {
      data.edge.fork = true;
      return *this;
    }

    EdgeBuilder& consumeEvent() {
      data.edge.consumeEvent = true;
      return *this;
    }

    operator Input::EdgeData() const {
      return data;
    }

    Input::EdgeData data;
  };

  struct NodeBuilder {
    NodeBuilder& button() {
      data.data.emplace<Input::Node::Button>();
      return *this;
    }

    NodeBuilder& axis1D() {
      data.data.emplace<Input::Node::Axis1D>();
      return *this;
    }

    NodeBuilder& axis2D() {
      data.data.emplace<Input::Node::Axis2D>();
      return *this;
    }

    NodeBuilder& emitEvent(Input::EventID id) {
      data.event = id;
      return *this;
    }

    operator Input::Node() const {
      return data;
    }

    Input::Node data;
  };

  class StateMachine;
  struct StateMachineBuilder;

  //Responsible for mapping platform inputs to state machine nodes/edges without containing
  //knowledge of the current input state
  class InputMapper {
  public:
    void addKeyMapping(PlatformInputID src, KeyMapID dst);
    //Add a mapping for an input that is already in axis format, like a mouse or joystick
    void addAxis1DMapping(PlatformInputID src, KeyMapID dst);
    void addAxis2DMapping(PlatformInputID src, KeyMapID dst);
    //Map a key to a direction, like making WASD equivalent to thumbstick input
    void addKeyAs1DRelativeMapping(PlatformInputID src, KeyMapID dst, float amount);
    void addKeyAs2DRelativeMapping(PlatformInputID src, KeyMapID dst, const glm::vec2& amount);

    EdgeTraverser onKeyDown(PlatformInputID key) const;
    EdgeTraverser onKeyUp(PlatformInputID key) const;
    //Relative or absolute is specified by the platform depending on what makes sense for the input device
    //The state machine will then infer the relative or absolute that wasn't specified
    EdgeTraverser onAxis1DRelative(PlatformInputID axis, float relative) const;
    EdgeTraverser onAxis1DAbsolute(PlatformInputID axis, float absolute) const;
    EdgeTraverser onAxis2DRelative(PlatformInputID axis, const glm::vec2& relative) const;
    EdgeTraverser onAxis2DAbsolute(PlatformInputID axis, const glm::vec2& relative) const;
    EdgeTraverser onTick(Timespan timeElapsed) const;

  private:
    template<class EdgeT>
    void addMapping(PlatformInputID src, KeyMapID dst, EdgeT&& edge);

    friend class StateMachine;
    //Properly handling cases of multiple input sources contributing to a single logical input
    //requires the state machine to have knowledge of these redundant keys. This way if multiple
    //keys corresponding to the same input are pressed then releasing one can still recognized that
    //another is still down.
    //This builds the association between the keymappings and the particular node indicies in the state machine,
    //binding this mapper to that state machine
    void bind(StateMachineBuilder& mapper);

    struct Mapping {
      EdgeTraverser traverser;
    };
    struct ReverseMapping {
      NodeIndex subnodeCount{};
      NodeIndex currentSubnode{};
    };
    std::unordered_map<PlatformInputID, Mapping> mappings;
    std::unordered_map<KeyMapID, ReverseMapping> reverseMappings;
  };

  struct StateMachineBuilder {
    StateMachineBuilder();
    //Building the state machine
    NodeIndex addNode(const Node& node);
    EdgeIndex addEdge(NodeIndex from, NodeIndex to, const Edge& edge);
    EdgeIndex addEdge(const EdgeData& data);

    std::vector<Node> nodes;
    std::vector<Edge> edges;
  };

  //Holds the current logical state of the input while relying on the InputMapper for knowing
  //how the platform inputs map to the logical state
  class StateMachine {
  public:
    static constexpr NodeIndex ROOT_NODE{};

    StateMachine(StateMachineBuilder&& builder, InputMapper&& mapper);
    StateMachine(StateMachine&&) = default;
    StateMachine(const StateMachine&) = default;

    //Sending inputs through the state machine
    void traverse(const EdgeTraverser& traverser);

    //Query input state
    bool isNodeActive(NodeIndex node) const;
    //Absolute axes are queried here, relative ones would be read through events
    float getAbsoluteAxis1D(NodeIndex node) const;
    glm::vec2 getAbsoluteAxis2D(NodeIndex node) const;

    const std::vector<Event>& readEvents() const;
    void clearEvents();

    const InputMapper& getMapper();

  private:
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<NodeIndex> activeNodes;
    std::vector<Event> publishedEvents;
    InputMapper mapper;
  };
}