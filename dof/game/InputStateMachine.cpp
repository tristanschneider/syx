#include "Precompile.h"
#include "InputStateMachine.h"

namespace Input {
  constexpr float UNSET1D = std::numeric_limits<float>::max();
  constexpr glm::vec2 UNSET2D{ UNSET1D, UNSET1D };

  constexpr EdgeTraverser NOOP_TRAVERSER{};

  NodeIndex StateMachine::addNode(const Node& node) {

  }

  EdgeIndex StateMachine::addEdge(NodeIndex from, NodeIndex to, const Edge& edge) {

  }

  void StateMachine::traverse(const EdgeTraverser& traverser) {

  }


  void InputMapper::bind(StateMachine& mapper) {

  }

  void InputMapper::addKeyMapping(PlatformInputID src, KeyMapID dst) {
    auto [platformMapping, isNewPlatform] = mappings.emplace(src, Mapping{});
    auto [reverseMapping, isNewReverse] = reverseMappings.emplace(dst, src);
    assert(isNewPlatform);
    EdgeTraverser& traverser = platformMapping->second.traverser;

    //Associate the platform input with the key, node indices will be filled in later
    platformMapping->second.key = dst;
    traverser.data.emplace<Edge::KeyDown>();
    //If there was already a mapping to this key, increment the counter for later
    if(!isNewReverse) {
      reverseMapping->second.subnodeCount++;
    }

    if(isNewReverse) {
    }
    else {
      platformMapping->second.key = dst;

      //auto existing = mappings.find(reverseMapping->second);
      //assert(existing != mappings.end());
      //if(existing != mappings.end()) {
      traverser.primaryNode = existing->first;
    }
  }

  void InputMapper::addAxis1DMapping(PlatformInputID src, KeyMapID dst) {

  }

  void InputMapper::addAxis2DMapping(PlatformInputID src, KeyMapID dst) {

  }

  void InputMapper::addKeyAs1DRelativeMapping(PlatformInputID src, KeyMapID dst, float amount) {

  }

  void InputMapper::addKeyAs2DRelativeMapping(PlatformInputID src, KeyMapID dst, const glm::vec2& amount) {

  }

  EdgeTraverser InputMapper::onKeyDown(PlatformInputID key) const {
    auto it = mappings.find(key);
    return it != mappings.end() ? it->second.traverser : NOOP_TRAVERSER;
  }

  EdgeTraverser InputMapper::onKeyUp(PlatformInputID key) const {
    auto it = mappings.find(key);
    return it != mappings.end() ? it->second.traverser : NOOP_TRAVERSER;
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