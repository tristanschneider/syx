#include "Precompile.h"
#include "InputStateMachine.h"
#include "GameMath.h"
#include "generics/LinkedList.h"

namespace gnx::LinkedList {
  template<>
  struct Traits<Input::Node> {
    static Input::NodeIndex& getIndex(Input::Node& node) { return node.edges; }
    static const Input::NodeIndex& getIndex(const Input::Node& node) { return node.edges; }
  };

  template<>
  struct Traits<Input::Edge> {
    static Input::EdgeIndex& getIndex(Input::Edge& edge) { return edge.edges; }
    static const Input::EdgeIndex& getIndex(const Input::Edge& edge) { return edge.edges; }
  };
};

namespace Input {
  constexpr EdgeTraverser NOOP_TRAVERSER{};

  constexpr EdgeTraverser::Axis1D valueToTraverser(float);
  constexpr EdgeTraverser::Axis2D valueToTraverser(glm::vec2);
  constexpr InputSourceRange::Axis1D edgeToRange(EdgeTraverser::Axis1D);
  constexpr InputSourceRange::Axis2D edgeToRange(EdgeTraverser::Axis2D);
  constexpr InputSourceRange::Button edgeToRange(EdgeTraverser::KeyUp);
  constexpr InputSourceRange::Button edgeToRange(EdgeTraverser::KeyDown);
  constexpr EdgeTraverser::Axis1D rangeToEdge(InputSourceRange::Axis1D);
  constexpr EdgeTraverser::Axis2D rangeToEdge(InputSourceRange::Axis2D);
  constexpr InputSourceRange::Axis1D eventToRange(Event::Axis1D);
  constexpr InputSourceRange::Axis2D eventToRange(Event::Axis2D);
  template<class T>
  struct EdgeToTraverser;
  template<>
  struct EdgeToTraverser<Edge::Absolute1D> {
    using TraverserT = EdgeTraverser::Axis1D;
    static constexpr auto AxisPtr = &EdgeTraverser::Axis1D::absolute;
  };
  template<>
  struct EdgeToTraverser<Edge::Delta1D> {
    using TraverserT = EdgeTraverser::Axis1D;
    static constexpr auto AxisPtr = &EdgeTraverser::Axis1D::delta;
  };
  template<>
  struct EdgeToTraverser<Edge::Absolute2D> {
    using TraverserT = EdgeTraverser::Axis2D;
    static constexpr auto AxisPtr = &EdgeTraverser::Axis2D::absolute;
  };
  template<>
  struct EdgeToTraverser<Edge::Delta2D> {
    using TraverserT = EdgeTraverser::Axis2D;
    static constexpr auto AxisPtr = &EdgeTraverser::Axis2D::delta;
  };
  template<>
  struct EdgeToTraverser<Edge::KeyDown> {
    using TraverserT = EdgeTraverser::KeyDown;
  };
  template<>
  struct EdgeToTraverser<Edge::KeyUp> {
    using TraverserT = EdgeTraverser::KeyUp;
  };

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
    eventDescriptions = std::move(builder.events);
    inputSources = std::move(builder.inputSources);
  }

  NodeIndex StateMachineBuilder::addNode(const EventDescription& event) {
    NodeIndex result = static_cast<NodeIndex>(nodes.size());
    Node& node = nodes.emplace_back();
    if(event.id != INVALID_EVENT) {
      node.event = static_cast<EventID>(events.size());
      events.push_back(event);
    }
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
    gnx::LinkedList::insertAtEnd(nodes[data.from], e, edges);

    return result;
  }

  struct CanTraverse {
    //Can traverse the button if the event types match (down to down)
    template<class ButtonEdgeT>
    bool canTraverseButton(const ButtonEdgeT&) {
      return std::get_if<typename EdgeToTraverser<ButtonEdgeT>::TraverserT>(&traverser.data) != nullptr;
    }

    template<class EdgeT>
    bool canTraverseAxis(const EdgeT& e) {
      using Mapping = EdgeToTraverser<EdgeT>;
      const auto* traverseAxis = std::get_if<typename Mapping::TraverserT>(&traverser.data);
      constexpr auto ptr = Mapping::AxisPtr;
      auto&& [edgeMin, edgeMax] = e.get();
      return traverseAxis && Math::between(traverseAxis->*ptr, edgeMin, edgeMax);
    }

    bool operator()(const Edge::Empty) { return true; }
    bool operator()(const Edge::Timeout& e) {
      //The time in the node added by the traverser is accumulated before calling this, compare the
      //total time rather than only the time added by the traverser
      return e.timeoutAfter <= timeInNode;
    }

    bool operator()(const Edge::KeyUp& e) { return canTraverseButton(e); }
    bool operator()(const Edge::KeyDown& e) { return canTraverseButton(e); }
    bool operator()(const Edge::Delta1D& e) { return canTraverseAxis(e); }
    bool operator()(const Edge::Delta2D& e) { return canTraverseAxis(e); }
    bool operator()(const Edge::Absolute1D& e) { return canTraverseAxis(e); }
    bool operator()(const Edge::Absolute2D& e) { return canTraverseAxis(e); }

    bool shouldTraverseEdge() {
      //Unconditional edge can always be traversed
      if(std::get_if<Edge::Empty>(&edge.data)) {
        return true;
      }
      //Key needs to match or be unset, then check the variants
      if(edge.key == INVALID_KEY || edge.key == traverser.key) {
        //See if the conditions within the variant are met
        return std::visit(*this, edge.data);
      }
      return false;
    }

    EdgeTraverser& traverser;
    const StateMachine& machine;
    const Edge& edge;
    Timespan timeInNode;
  };

  void enterNode(Node& node) {
    node.isActive = true;
  }

  void exitNode(Node& node) {
    node.isActive = false;
    node.timeActive = {};
  }

  //Fill the missing absolute or relative part of the input. Then regardless of if an edge is traversed,
  //commit the change to the input sources so they're always up to date
  struct FillMissingInputAndCommit {
    template<class Axis>
    void doAxis(Axis& axis) {
      const auto& range = std::get<decltype(edgeToRange(axis))>(traverser.inputSourceRange.data);
      auto& axisSource = sources.get(range);

      //Get or compute the absolute value of this input source
      const auto thisSourceAbsolute = axis.absolute == Axis::UNSET ?
        axis.delta + axisSource[traverser.inputSource] :
        axis.absolute;
      //Since this is the only value changing, delta of this input source is the same delta as if it were accumulated
      //across all input sources
      axis.delta = thisSourceAbsolute - axisSource[traverser.inputSource];

      //Update this specific input source
      axisSource[traverser.inputSource] = thisSourceAbsolute;

      //Compute the absolute across all input sources including the latest change and put that in the traverser
      axis.absolute = {};
      for(uint32_t i = range.begin; i < range.end; ++i) {
        axis.absolute += axisSource[i];
      }
    }

    void doButton(bool isDown) {
      const auto& range = std::get<InputSourceRange::Button>(traverser.inputSourceRange.data);
      const bool oldButtonState = sources.getAccumulatedInput(range);
      sources.buttons[traverser.inputSource] = isDown;
      const bool newButtonState = sources.getAccumulatedInput(range);
      //If this didn't change the global state then it doesn't need to be emitted as an event
      //For example, a key down event of a second key while the first mapped to the same key is already down
      if(oldButtonState == newButtonState) {
        traverser.data.emplace<EdgeTraverser::Empty>();
      }
    }

    void operator()(EdgeTraverser::Axis1D& axis1D) {
      doAxis(axis1D);
    }
    void operator()(EdgeTraverser::Axis2D& axis2D) {
      doAxis(axis2D);
    }
    void operator()(EdgeTraverser::KeyDown&) {
      doButton(true);
    }
    void operator()(EdgeTraverser::KeyUp&) {
      doButton(false);
    }
    void operator()(EdgeTraverser::Tick) {}
    void operator()(EdgeTraverser::Empty) {}

    InputSources& sources;
    EdgeTraverser& traverser;
  };

  struct FillEventData {
    template<class Axis>
    void fillAxis(Axis& axis) {
      using RangeT = decltype(eventToRange(axis));
      using TraverserT = decltype(rangeToEdge(RangeT{}));
      if(eventInput == traverser.inputSourceRange) {
        const auto& t = std::get<TraverserT>(traverser.data);
        axis.absolute = t.absolute;
        axis.delta = t.delta;
      }
      else {
        axis.absolute = sources.getAccumulatedInput(std::get<RangeT>(eventInput.data));
      }
    }

    void operator()(Event::Axis1D& axis) {
      fillAxis(axis);
    }
    void operator()(Event::Axis2D& axis) {
      fillAxis(axis);
    }
    void operator()(Event::Empty) {}

    const EdgeTraverser& traverser;
    const InputSourceRange& eventInput;
    const InputSources& sources;
  };

  void StateMachine::traverse(EdgeTraverser&& traverser) {
    //Early out for no-op case
    if(!traverser) {
      return;
    }

    std::visit(FillMissingInputAndCommit{ inputSources, traverser }, traverser.data);
    //Visit above may also map to empty, so exit in that case as well
    if(!traverser) {
      return;
    }

    const EdgeTraverser::Tick* timeElapsed = std::get_if<EdgeTraverser::Tick>(&traverser.data);

    for(size_t i = 0; i < activeNodes.size();) {
      NodeIndex active = activeNodes[i];
      Node& node = nodes[active];
      if(timeElapsed) {
        node.timeActive += timeElapsed->timeElapsed;
      }
      bool removedActiveNode = false;
      bool eventConsumed = false;
      //Try all edges going out of active nodes
      gnx::LinkedList::foreach(edges, node.edges, [&](Edge& edge) {
        CanTraverse canTraverse{ traverser, *this, edge, node.timeActive };
        if(!canTraverse.shouldTraverseEdge()) {
          return true;
        }

        if(edge.consumeEvent) {
          eventConsumed = true;
        }

        Node& destination = nodes[edge.to];
        //ROOT special case means exit the machine, so no node entry logic is needed
        if(edge.to != ROOT_NODE && !isNodeActive(edge.to)) {
          enterNode(destination);
          addToActiveList(edge.to, activeNodes);
          if(destination.event != INVALID_EVENT) {
            const EventDescription& description = eventDescriptions[destination.event];
            Event e;
            e.id = description.id;
            e.timeInNode = node.timeActive;
            e.data = description.data;
            std::visit(FillEventData{ traverser, description.inputSource, inputSources }, e.data);
            publishedEvents.push_back(std::move(e));
          }
        }

        //Either this is an exit condition or the state moved from one edge to another, deactivate source node
        //It should only remain if the execution is forking meaning both the source and destination are now active
        if(active != ROOT_NODE && (edge.to == ROOT_NODE || !edge.fork)) {
          exitNode(node);
          removeFromActiveList(activeNodes.begin() + i, activeNodes);
          removedActiveNode = true;

          //False stops traversing edges of this node now that it has been deactivated
          return false;
        }
        if(edge.consumeEvent) {
          eventConsumed = true;
          return false;
        }
        //Continue traversing edges (return true) unless this edge consumed the event
        return eventConsumed;
      });

      //Stop traversing active nodes if the event was consumed
      if(eventConsumed) {
        return;
      }
      if(!removedActiveNode) {
        ++i;
      }
    }
  }

  bool StateMachine::isNodeActive(NodeIndex nodeIndex) const {
    return nodes[nodeIndex].isActive;
  }

  float StateMachine::getAbsoluteAxis1D(const InputSourceRange& source) const {
    float result{};
    if(const auto* range = std::get_if<InputSourceRange::Axis1D>(&source.data)) {
      for(InputSourceID i = range->begin; i < range->end; ++i) {
        result += inputSources.axes1D[i];
      }
    }
    return result;
  }

  glm::vec2 StateMachine::getAbsoluteAxis2D(const InputSourceRange& source) const {
    glm::vec2 result{ 0, 0 };
    if(const auto* range = std::get_if<InputSourceRange::Axis2D>(&source.data)) {
      for(InputSourceID i = range->begin; i < range->end; ++i) {
        result += inputSources.axes2D[i];
      }
    }
    return result;
  }

  bool StateMachine::getButtonPressed(const InputSourceRange& source) const {
    bool result = false;
    if(const auto* range = std::get_if<InputSourceRange::Button>(&source.data)) {
      for(InputSourceID i = range->begin; i < range->end; ++i) {
        result = result || inputSources.buttons[i];
      }
    }
    return result;
  }

  const std::vector<Event>& StateMachine::readEvents() const {
    return publishedEvents;
  }

  void StateMachine::clearEvents() {
    publishedEvents.clear();
  }

  const InputMapper& StateMachine::getMapper() const {
    return mapper;
  }

  struct AddInputSource {
    template<class T>
    void addTo(std::vector<T>& container) {
      InputSourceRange::Range& range = inout.range();
      range.begin = static_cast<InputSourceID>(container.size());
      const size_t sourceCount = static_cast<size_t>(range.end);
      container.insert(container.end(), sourceCount, {});
      range.end = static_cast<InputSourceID>(container.size());
    }

    void operator()(const EdgeTraverser::KeyUp) {}
    void operator()(const EdgeTraverser::Tick) {}
    void operator()(const EdgeTraverser::Empty) {}
    void operator()(const EdgeTraverser::Axis1D&) {
      addTo(container.axes1D);
    }
    void operator()(const EdgeTraverser::Axis2D&) {
      addTo(container.axes2D);
    }
    void operator()(const EdgeTraverser::KeyDown&) {
      addTo(container.buttons);
    }

    InputSources& container;
    InputSourceRange& inout;
  };

  void InputMapper::bind(StateMachineBuilder& machine) {
    //Add input sources for all keys
    for(auto&& [keyMapKey, mapping] : reverseMappings) {
      //Every key event should have the same type, so grab one of them and add for it
      if(auto it = mappings.find(mapping.platformKey); it != mappings.end()) {
        std::visit(AddInputSource{ machine.inputSources, mapping.inputSourceRange }, it->second.traverser.data);
      }
    }
    //Set the particular input source for each platform key
    for(auto&& [platformkey, traverser] : mappings) {
      auto it = reverseMappings.find(traverser.traverser.key);
      //Should always exist
      if(it == reverseMappings.end()) {
        continue;
      }
      traverser.traverser.inputSource = it->second.inputSourceRange.range().begin + it->second.currentInputSource++;
      traverser.traverser.inputSourceRange = it->second.inputSourceRange;
    }
    //Create input sources for events
    for(EventDescription& event : machine.events) {
      //Key was stored in the range. Reassign the range to the actual key
      const KeyMapID key = static_cast<KeyMapID>(event.inputSource.range().begin);
      if(auto reverse = reverseMappings.find(key); reverse != reverseMappings.end()) {
        event.inputSource = reverse->second.inputSourceRange;
      }
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
    //The end indicates how many inputs of this type there are, the range will be shifted and the begin
    //set after adding all mappings
    using RangeT = decltype(edgeToRange(edge));
    if(!isNewReverse) {
      std::get<RangeT>(reverseMapping->second.inputSourceRange.data).end++;
    }
    else {
      reverseMapping->second.platformKey = src;
      //Start with 1 for this mapping, then increment for each additional mapping
      reverseMapping->second.inputSourceRange.data.emplace<RangeT>(InputSourceRange::Range{ 0, 1 });
    }
  }

  PlatformInputID InputMapper::getUniquePlatformKey(KeyMapID forKey) {
    return static_cast<PlatformInputID>(std::hash<KeyMapID>()(forKey));
  }

  void InputMapper::addPassthroughKeyMapping(KeyMapID dst) {
    addKeyMapping(getUniquePlatformKey(dst), dst);
  }

  void InputMapper::addPassthroughAxis1D(KeyMapID dst) {
    addAxis1DMapping(getUniquePlatformKey(dst), dst);
  }

  void InputMapper::addPassthroughAxis2D(KeyMapID dst) {
    addAxis2DMapping(getUniquePlatformKey(dst), dst);
  }

  void InputMapper::addKeyMapping(PlatformInputID src, KeyMapID dst) {
    addMapping(src, dst, EdgeTraverser::KeyDown{});
  }

  void InputMapper::addAxis1DMapping(PlatformInputID src, KeyMapID dst) {
    addMapping(src, dst, EdgeTraverser::Axis1D{});
  }

  void InputMapper::addAxis2DMapping(PlatformInputID src, KeyMapID dst) {
    addMapping(src, dst, EdgeTraverser::Axis2D{});
  }

  void InputMapper::addKeyAs1DRelativeMapping(PlatformInputID src, KeyMapID dst, float amount) {
    EdgeTraverser::Axis1D e;
    e.delta = amount;
    addMapping(src, dst, std::move(e));
  }

  void InputMapper::addKeyAs2DRelativeMapping(PlatformInputID src, KeyMapID dst, const glm::vec2& amount) {
    EdgeTraverser::Axis2D e;
    e.absolute = amount;
    addMapping(src, dst, std::move(e));
  }

  EdgeTraverser InputMapper::onPassthroughKeyDown(KeyMapID key) const {
    auto it = reverseMappings.find(key);
    return it != reverseMappings.end() ? onKeyDown(it->second.platformKey) : EdgeTraverser{};
  }

  EdgeTraverser InputMapper::onPassthroughAxis2DAbsolute(KeyMapID axis, const glm::vec2& absolute) const {
    auto it = reverseMappings.find(axis);
    return it != reverseMappings.end() ? onAxis2DAbsolute(it->second.platformKey, absolute) : EdgeTraverser{};
  }

  //Keys are stored as key down events so this case can return directly
  EdgeTraverser InputMapper::onKeyDown(PlatformInputID key) const {
    auto it = mappings.find(key);
    return it != mappings.end() ? it->second.traverser : NOOP_TRAVERSER;
  }

  template<class T>
  void undoDirectionKey(T& data) {
    data.absolute = {};
    data.delta = -data.delta;
  }

  //Keys are stored as key down so this needs to flip it to key up before returning
  EdgeTraverser InputMapper::onKeyUp(PlatformInputID key) const {
    if(auto it = mappings.find(key); it != mappings.end()) {
      EdgeTraverser result = it->second.traverser;
      if(std::get_if<EdgeTraverser::KeyDown>(&result.data)) {
        result.data.emplace<EdgeTraverser::KeyUp>();
      }
      //Flip directions since the key is being released
      else if(auto* d1 = std::get_if<EdgeTraverser::Axis1D>(&result.data)) {
        undoDirectionKey(*d1);
      }
      else if(auto* d2 = std::get_if<EdgeTraverser::Axis2D>(&result.data)) {
        undoDirectionKey(*d2);
      }
      return result;
    }
    return NOOP_TRAVERSER;
  }

  template<class ValueT>
  EdgeTraverser getAxisMapping(
    const std::unordered_map<PlatformInputID, InputMapper::Mapping>& mappings,
    PlatformInputID axis,
    const ValueT& value,
    bool isRelative
  ) {
    using TraverserT = decltype(valueToTraverser(value));
    if(auto it = mappings.find(axis); it != mappings.end()) {
      EdgeTraverser result = it->second.traverser;
      if(auto* data = std::get_if<TraverserT>(&result.data)) {
        if(isRelative) {
          data->delta = value;
          data->absolute = TraverserT::UNSET;
        }
        else {
          data->delta = TraverserT::UNSET;
          data->absolute = value;
        }
        return result;
      }
    }
    return NOOP_TRAVERSER;
  }

  EdgeTraverser InputMapper::onAxis1DRelative(PlatformInputID axis, float relative) const {
    return getAxisMapping(mappings, axis, relative, true);
  }

  EdgeTraverser InputMapper::onAxis1DAbsolute(PlatformInputID axis, float absolute) const {
    return getAxisMapping(mappings, axis, absolute, false);
  }

  EdgeTraverser InputMapper::onAxis2DRelative(PlatformInputID axis, const glm::vec2& relative) const {
    return getAxisMapping(mappings, axis, relative, true);
  }

  EdgeTraverser InputMapper::onAxis2DAbsolute(PlatformInputID axis, const glm::vec2& absolute) const {
    return getAxisMapping(mappings, axis, absolute, false);
  }

  EdgeTraverser InputMapper::onTick(Timespan timeElapsed) const {
    EdgeTraverser result;
    result.data.emplace<EdgeTraverser::Tick>(timeElapsed);
    return result;
  }

  InputSourceRange InputMapper::getPlatformInputSource(PlatformInputID key) const {
    auto it = mappings.find(key);
    return it != mappings.end() ? it->second.traverser.inputSourceRange : InputSourceRange{};
  }

  InputSourceRange InputMapper::getInputSource(KeyMapID key) const {
    auto it = reverseMappings.find(key);
    return it != reverseMappings.end() ? it->second.inputSourceRange : InputSourceRange{};
  }
};