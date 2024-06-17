#pragma once

#include "glm/vec2.hpp"

namespace Input {
  using PlatformInputID = uint32_t;
  using EventID = uint32_t;
  using KeyMapID = uint32_t;
  using NodeIndex = uint32_t;
  using EdgeIndex = uint32_t;
  using Timespan = uint32_t;
  using InputSourceID = uint32_t;

  constexpr NodeIndex INVALID_NODE = std::numeric_limits<NodeIndex>::max();
  constexpr EdgeIndex INVALID_EDGE = std::numeric_limits<EdgeIndex>::max();
  constexpr EventID INVALID_EVENT = std::numeric_limits<EventID>::max();
  constexpr KeyMapID INVALID_KEY = std::numeric_limits<KeyMapID>::max();

  struct InputSourceRange {
    struct Range {
      bool operator<=>(const Range&) const = default;

      //Beginning after end means any range-based traversal will no-op
      InputSourceID begin{ 1 };
      InputSourceID end{ 0 };
    };
    struct Button : Range{
      bool operator<=>(const Button&) const = default;
    };
    struct Axis1D : Range{
      bool operator<=>(const Axis1D&) const = default;
    };
    struct Axis2D : Range{
      bool operator<=>(const Axis2D&) const = default;
    };
    using Variant = std::variant<Button, Axis1D, Axis2D>;

    bool operator<=>(const InputSourceRange&) const = default;

    operator bool() const {
      return std::visit([](const auto& r) { return r.begin <= r.end; }, data);
    }

    Range& range() {
      return std::visit([](auto& r) -> Range& { return r; }, data);
    }

    const Range& range() const {
      return std::visit([](const auto& r) -> const Range& { return r; }, data);
    }

    Variant data;
  };

  struct InputSources {
    auto& get(const InputSourceRange::Button&) { return buttons; }
    auto& get(const InputSourceRange::Axis1D&) { return axes1D; }
    auto& get(const InputSourceRange::Axis2D&) { return axes2D; }
    const auto& get(const InputSourceRange::Button&) const { return buttons; }
    const auto& get(const InputSourceRange::Axis1D&) const { return axes1D; }
    const auto& get(const InputSourceRange::Axis2D&) const { return axes2D; }

    //If any button is down the state is considered down
    bool getAccumulatedInput(const InputSourceRange::Button& range) const {
      for(uint32_t i = range.begin; i < range.end; ++i) {
        if(buttons[i]) {
          return true;
        }
      }
      return false;
    }
    //Axes are accumulated, meaning if multiple thumbsticks were mapped to an axis the final value could go beyond the expected [-1,1]
    template<class SourceRange>
    auto getAccumulatedInput(const SourceRange& range) const {
      const auto& inputs = get(range);
      using ValueT = std::decay_t<decltype(inputs[0])>;
      ValueT result{};
      for(uint32_t i = range.begin; i < range.end; ++i) {
        result += inputs[i];
      }
      return result;
    }

    std::vector<bool> buttons;
    std::vector<float> axes1D;
    std::vector<glm::vec2> axes2D;
  };

  struct Event {
    struct Empty {};
    //Delta values are only applicable if the event is triggered by an edge that changes the axis
    //Otherwise the deltas are zero while the absolute is populated
    struct Axis1D {
      float delta{};
      float absolute{};
    };
    struct Axis2D {
      glm::vec2 delta{ 0 };
      glm::vec2 absolute{ 0 };
    };

    float getAxis1DRelative() const {
      auto* result = std::get_if<Axis1D>(&data);
      return result ? result->delta : 0.0f;
    }

    float getAxis1DAbsolute() const {
      auto* result = std::get_if<Axis1D>(&data);
      return result ? result->absolute : 0.0f;
    }

    EventID id{};
    //Time spent in the previous node before traversing the edge that triggered this event
    Timespan timeInNode{};
    //Additional optional data that can be associated with the event
    using Variant = std::variant<Empty, Axis1D, Axis2D>;
    Variant data;
  };

  struct EventDescription {
    EventID id{ INVALID_EVENT };
    InputSourceRange inputSource;
    Event::Variant data;
  };

  //A node is an abstract representation of a particular input state in the machine
  //It does not in itself hold any key states. Edges connected to it determine the types of input
  //events that can cause the active nodes to change.
  struct Node {
    //Intrusive linked list of edges
    EdgeIndex edges{ INVALID_EDGE };
    //Index of EventDescription in events list
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
      auto get() const { return std::tie(minDelta, maxDelta); }

      float minDelta{};
      float maxDelta{};
    };
    struct Delta2D {
      auto get() const { return std::tie(minDelta, maxDelta); }

      glm::vec2 minDelta{ 0 };
      glm::vec2 maxDelta{ 0 };
    };
    struct Absolute1D {
      auto get() const { return std::tie(minAbsolute, maxAbsolute); }

      float minAbsolute{};
      float maxAbsolute{};
    };
    struct Absolute2D {
      auto get() const { return std::tie(minAbsolute, maxAbsolute); }

      glm::vec2 minAbsolute{ 0 };
      glm::vec2 maxAbsolute{ 0 };
    };

    operator bool() const {
      return std::get_if<Empty>(&data) == nullptr;
    }

    using Variant = std::variant<Empty, KeyDown, KeyUp, Timeout, Delta1D, Delta2D, Absolute1D, Absolute2D>;
    KeyMapID key{ INVALID_KEY };
    Variant data;
    //Stop further traversal immediately if this edge is traversed
    bool consumeEvent{};
    //When being traversed, this will leave the source node active rather than moving
    //exclusively to the destination node
    bool fork{};
    NodeIndex to{ INVALID_NODE };
    //Intrusive linked list of other edges attached to the node
    EdgeIndex edges{ INVALID_EDGE };
  };

  //This is the input to the state machine that is matched against edges to see if they are applicable
  //If it is for an input it refers to a key as well as the key's input source
  //A given KeyMapID may have multiple PlatformInputIDs corresponding to it. The KeyMapID represents the
  //sum of all of the PlatformInputIDs.The InputSourceRange is used to know which sources belong to the key
  //then the `inputSource` determines which source to write the change in the traverser to.
  //The input source change is applied regardless of if the edge is traversed
  struct EdgeTraverser {
    struct Empty {};
    struct KeyDown {};
    struct KeyUp {};
    struct Tick {
      Timespan timeElapsed{};
    };
    struct Axis1D {
      constexpr static float UNSET = std::numeric_limits<float>::max();
      float delta{ UNSET };
      float absolute{ UNSET };
    };
    struct Axis2D {
      constexpr static glm::vec2 UNSET{ std::numeric_limits<float>::max() };
      glm::vec2 delta{ UNSET };
      glm::vec2 absolute{ UNSET };
    };

    operator bool() const {
      return std::get_if<Empty>(&data) == nullptr;
    }

    //The logical key that is being pressed
    KeyMapID key{ INVALID_KEY };
    //The input source that should store the information related to that key
    InputSourceID inputSource{};
    //The range of all inputs corresponding to the key
    InputSourceRange inputSourceRange;
    using Variant = std::variant<Empty, KeyDown, KeyUp, Tick, Axis1D, Axis2D>;
    Variant data;
  };

  struct EdgeData {
    NodeIndex from{};
    NodeIndex to{};
    Edge edge;
  };

  struct EdgeBuilder {
    EdgeBuilder& from(Input::NodeIndex id) {
      data.from = id;
      return *this;
    }

    EdgeBuilder& to(Input::NodeIndex id) {
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

    EdgeBuilder& absolute1D(Input::KeyMapID id, float min, float max) {
      data.edge.key = id;
      data.edge.data.emplace<Input::Edge::Absolute1D>(min, max);
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

    EdgeBuilder& timeout(Input::Timespan time) {
      data.edge.data.emplace<Input::Edge::Timeout>(time);
      return *this;
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
    NodeBuilder& emitEvent(Input::EventID id) {
      data.id = id;
      return *this;
    }

    NodeBuilder& emitAxis1DEvent(Input::EventID id, Input::KeyMapID key) {
      data.id = id;
      //Key is temporarily stored here before being resolved upon creating the state machine
      data.inputSource.data.emplace<InputSourceRange::Axis1D>(InputSourceRange::Range{ key, key });
      data.data.emplace<Event::Axis1D>();
      return *this;
    }

    NodeBuilder& emitAxis2DEvent(Input::EventID id, Input::KeyMapID key) {
      data.id = id;
      data.inputSource.data.emplace<InputSourceRange::Axis2D>(InputSourceRange::Range{ key, key });
      data.data.emplace<Event::Axis2D>();
      return *this;
    }

    operator EventDescription() const {
      return data;
    }

    EventDescription data;
  };

  class StateMachine;
  struct StateMachineBuilder;

  //Responsible for mapping platform inputs to state machine nodes/edges without containing
  //knowledge of the current input state
  class InputMapper {
  public:
    struct Mapping {
      EdgeTraverser traverser;
    };
    struct ReverseMapping {
      PlatformInputID platformKey;
      InputSourceRange inputSourceRange;
      InputSourceID currentInputSource{};
    };

    //Key that has no associated PlatformInputID but is intended to be used with onPassthroughKeyDown
    void addPassthroughKeyMapping(KeyMapID dst);
    void addPassthroughAxis1D(KeyMapID dst);
    void addPassthroughAxis2D(KeyMapID dst);
    void addKeyMapping(PlatformInputID src, KeyMapID dst);
    //Add a mapping for an input that is already in axis format, like a mouse or joystick
    void addAxis1DMapping(PlatformInputID src, KeyMapID dst);
    void addAxis2DMapping(PlatformInputID src, KeyMapID dst);
    //Map a key to a direction, like making WASD equivalent to thumbstick input
    void addKeyAs1DRelativeMapping(PlatformInputID src, KeyMapID dst, float amount);
    void addKeyAs2DRelativeMapping(PlatformInputID src, KeyMapID dst, const glm::vec2& amount);

    EdgeTraverser onPassthroughKeyDown(KeyMapID key) const;
    EdgeTraverser onPassthroughKeyUp(KeyMapID key) const;
    EdgeTraverser onPassthroughAxis2DAbsolute(KeyMapID axis, const glm::vec2& absolute) const;
    EdgeTraverser onKeyDown(PlatformInputID key) const;
    EdgeTraverser onKeyUp(PlatformInputID key) const;
    //Relative or absolute is specified by the platform depending on what makes sense for the input device
    //The state machine will then infer the relative or absolute that wasn't specified
    EdgeTraverser onAxis1DRelative(PlatformInputID axis, float relative) const;
    EdgeTraverser onAxis1DAbsolute(PlatformInputID axis, float absolute) const;
    EdgeTraverser onAxis2DRelative(PlatformInputID axis, const glm::vec2& relative) const;
    EdgeTraverser onAxis2DAbsolute(PlatformInputID axis, const glm::vec2& relative) const;
    EdgeTraverser onTick(Timespan timeElapsed) const;
    InputSourceRange getPlatformInputSource(PlatformInputID key) const;
    InputSourceRange getInputSource(KeyMapID key) const;

  private:
    template<class EdgeT>
    void addMapping(PlatformInputID src, KeyMapID dst, EdgeT&& edge);
    PlatformInputID getUniquePlatformKey(KeyMapID forKey);

    friend class StateMachine;
    //Properly handling cases of multiple input sources contributing to a single logical input
    //requires the state machine to have knowledge of these redundant keys. This way if multiple
    //keys corresponding to the same input are pressed then releasing one can still recognized that
    //another is still down.
    //This builds the association between the keymappings and the particular node indicies in the state machine,
    //binding this mapper to that state machine
    void bind(StateMachineBuilder& mapper);

    std::unordered_map<PlatformInputID, Mapping> mappings;
    std::unordered_map<KeyMapID, ReverseMapping> reverseMappings;
  };

  struct StateMachineBuilder {
    StateMachineBuilder();
    //Building the state machine
    NodeIndex addNode(const EventDescription& data);
    EdgeIndex addEdge(NodeIndex from, NodeIndex to, const Edge& edge);
    EdgeIndex addEdge(const EdgeData& data);

    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<EventDescription> events;
    InputSources inputSources;
  };

  //Holds the current logical state of the input while relying on the InputMapper for knowing
  //how the platform inputs map to the logical state
  class StateMachine {
  public:
    static constexpr NodeIndex ROOT_NODE{};

    StateMachine() = default;
    StateMachine(StateMachineBuilder&& builder, InputMapper&& mapper);
    StateMachine(StateMachine&&) = default;
    StateMachine(const StateMachine&) = default;

    StateMachine& operator=(const StateMachine&) = default;
    StateMachine& operator=(StateMachine&&) = default;

    //Sending inputs through the state machine
    void traverse(EdgeTraverser&& traverser);

    //Query input state
    bool isNodeActive(NodeIndex node) const;
    //Absolute axes are queried here, relative ones would be read through events
    float getAbsoluteAxis1D(const InputSourceRange& source) const;
    glm::vec2 getAbsoluteAxis2D(const InputSourceRange& source) const;
    bool getButtonPressed(const InputSourceRange& source) const;

    const std::vector<Event>& readEvents() const;
    void clearEvents();

    const InputMapper& getMapper() const;

  private:
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<NodeIndex> activeNodes;
    std::vector<EventDescription> eventDescriptions;
    std::vector<Event> publishedEvents;
    InputMapper mapper;
    InputSources inputSources;
  };
}