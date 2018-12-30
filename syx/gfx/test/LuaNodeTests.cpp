#include "Precompile.h"
#include "test/TestRegistry.h"

#include "lua/LuaNode.h"
#include "lua/LuaCompositeNodes.h"
#include <numeric>

namespace Test {
  using namespace Lua;

  struct Tracker {
    Tracker() { reset(); ++defaultCtors; }
    Tracker(const Tracker& rhs) { reset(); _accumulate(rhs); ++copyCtors; }
    Tracker(Tracker&& rhs) { reset(); _accumulate(rhs); ++moveCtors; }
    ~Tracker() { ++dtors; ++globalDtors; }
    Tracker& operator=(const Tracker& rhs) { _accumulate(rhs); ++copies; return *this; }
    Tracker& operator=(Tracker&& rhs) { _accumulate(rhs); ++copies; return *this; }
    bool operator==(const Tracker& rhs) const {
      return defaultCtors == rhs.defaultCtors
        && copyCtors == rhs.copyCtors
        && moveCtors == rhs.moveCtors
        && copies == rhs.copies
        && moves == rhs.moves
        && dtors == rhs.dtors;
    }
    bool operator!=(const Tracker& rhs) const {
      return !(*this == rhs);
    }
    bool validCtor() const { return (defaultCtors + copyCtors + moveCtors) == 1; }
    bool validDtor() const { return dtors == 1; }
    void reset() { defaultCtors = copyCtors = moveCtors = copies = moves = dtors = 0; }

    static void resetDtors() { globalDtors = 0; }

    //It may not be safe to look at the object after destruction, so dtors must be counted separately from instances
    static size_t globalDtors;

    uint8_t defaultCtors;
    uint8_t copyCtors;
    uint8_t moveCtors;
    uint8_t copies;
    uint8_t moves;
    uint8_t dtors;

  private:
    void _accumulate(const Tracker& t) {
      defaultCtors += t.defaultCtors;
      copyCtors += t.copyCtors;
      moveCtors += t.moveCtors;
      copies += t.copies;
      moves += t.moves;
      dtors += t.dtors;
    }
  };

  size_t Tracker::globalDtors = 0;

  class TrackerNode : public TypedNode<Tracker> {
    using TypedNode::TypedNode;
    void _readFromLua(lua_State*, void*) const override {}
    void _writeToLua(lua_State*, const void*) const override {}
  };

  class UniqueTrackerNode : public UniquePtrNode<Tracker> {
    using UniquePtrNode::UniquePtrNode;
    void _readFromLua(lua_State*, void*) const override {}
    void _writeToLua(lua_State*, const void*) const override {}
  };

  struct NodeTestObject {
    virtual void reset() {
      const auto values = getValues();
      std::for_each(values.begin(), values.end(), [](Tracker* v) { v->reset(); });
      Tracker::resetDtors();
    }
    virtual size_t defaultCtorCount() const {
      const auto values = getValues();
      return std::accumulate(values.begin(), values.end(), 0, [](size_t result, const Tracker* v) { return result + v->defaultCtors; });
    }
    virtual size_t copyCtorCount() const {
      const auto values = getValues();
      return std::accumulate(values.begin(), values.end(), 0, [](size_t result, const Tracker* v) { return result + v->copyCtors; });
    }
    virtual size_t moveCtorCount() const {
      const auto values = getValues();
      return std::accumulate(values.begin(), values.end(), 0, [](size_t result, const Tracker* v) { return result + v->moveCtors; });
    }
    virtual size_t copyCount() const {
      const auto values = getValues();
      return std::accumulate(values.begin(), values.end(), 0, [](size_t result, const Tracker* v) { return result + v->copies; });
    }
    virtual size_t moveCount() const {
      const auto values = getValues();
      return std::accumulate(values.begin(), values.end(), 0, [](size_t result, const Tracker* v) { return result + v->moves; });
    }
    virtual size_t dtorCount() const {
      return Tracker::globalDtors;
      //const auto values = getValues();
      //return std::accumulate(values.begin(), values.end(), 0, [](size_t result, const Tracker* v) { return result + v->dtors; });
    }
    virtual std::vector<const Tracker*> getValues() const {
      const std::vector<Tracker*> values = const_cast<NodeTestObject*>(this)->getValues();
      std::vector<const Tracker*> result;
      result.reserve(values.size());
      for(const Tracker* value : values) {
        result.push_back(value);
      }
      return result;
    }
    virtual std::vector<Tracker*> getValues() = 0;
    virtual std::unique_ptr<Node> getNode() = 0;
    size_t avg(size_t count) const { return count/getValues().size(); }
  };

  struct SingleObj : public NodeTestObject {
    Tracker value;

    std::unique_ptr<Node> getNode() override {
      auto root = makeRootNode(NodeOps(""));
      makeNode<TrackerNode>(NodeOps(*root, "value", Util::offsetOf(*this, value)));
      return root;
    }
    std::vector<Tracker*> getValues() override {
      return { &value };
    }
  };

  struct DoubleObj : public NodeTestObject {
    Tracker valueA, valueB;

    std::unique_ptr<Node> getNode() override {
      auto root = makeRootNode(NodeOps(""));
      makeNode<TrackerNode>(NodeOps(*root, "valueA", Util::offsetOf(*this, valueA)));
      makeNode<TrackerNode>(NodeOps(*root, "valueB", Util::offsetOf(*this, valueB)));
      return root;
    }
    std::vector<Tracker*> getValues() override {
      return { &valueA, &valueB };
    }
  };

  struct PaddedObj : public NodeTestObject {
    Tracker valueA;
    uint8_t padding[64];
    Tracker valueB;

    std::unique_ptr<Node> getNode() override {
      auto root = makeRootNode(NodeOps(""));
      makeNode<TrackerNode>(NodeOps(*root, "valueA", Util::offsetOf(*this, valueA)));
      makeNode<TrackerNode>(NodeOps(*root, "valueB", Util::offsetOf(*this, valueB)));
      return root;
    }
    std::vector<Tracker*> getValues() override {
      return { &valueA, &valueB };
    }
  };

  struct UnusedFieldObj : public DoubleObj {
    std::unique_ptr<Node> getNode() override {
      auto root = makeRootNode(NodeOps(""));
      makeNode<TrackerNode>(NodeOps(*root, "valueB", Util::offsetOf(*this, valueB)));
      return root;
    }
    std::vector<Tracker*> getValues() override {
      return { &valueB };
    }
  };

  struct UniquePtrObj : public NodeTestObject {
    std::unique_ptr<Tracker> value;

    UniquePtrObj(): value(std::make_unique<Tracker>()) {}
    UniquePtrObj(const UniquePtrObj& rhs): value(std::make_unique<Tracker>(*rhs.value)) {}
    UniquePtrObj(UniquePtrObj&&) = default;
    UniquePtrObj& operator=(const UniquePtrObj& rhs) { value = std::make_unique<Tracker>(*rhs.value); return *this; }
    UniquePtrObj& operator=(UniquePtrObj&&) = default;

    std::unique_ptr<Node> getNode() override {
      auto root = makeRootNode(NodeOps(""));
      makeNode<UniqueTrackerNode>(NodeOps(*root, "value", Util::offsetOf(*this, value)));
      return root;
    }
    std::vector<Tracker*> getValues() override {
      return { value.get() };
    }
  };

  using TestObjFactory = std::function<std::unique_ptr<NodeTestObject>()>;

  void Node_DefaultConstructAndDestroy_CtorAndDtorAreCalled(const TestObjFactory& factory) {
    std::unique_ptr<NodeTestObject> obj = factory();
    auto node = obj->getNode();

    obj->reset();
    node->defaultConstruct(obj.get());
    TEST_ASSERT(obj->avg(obj->defaultCtorCount()) == 1, "Default constructor should be called once");

    obj->reset();
    node->destruct(obj.get());
    TEST_ASSERT(obj->avg(obj->dtorCount()) == 1, "Destructor should be called once");
  }

  void Node_CopyToAndFromBuffer_CopyCalledTwice(const TestObjFactory& factory) {
    std::unique_ptr<NodeTestObject> obj = factory();
    auto node = obj->getNode();
    std::vector<uint8_t> buffer(node->size());

    obj->reset();
    //Once to call constructor on elements in buffer
    node->copyConstructToBuffer(obj.get(), buffer.data());
    //Now that constructor has been called, it is appropriate to call assignment operator
    node->copyToBuffer(obj.get(), buffer.data());
    node->copyFromBuffer(obj.get(), buffer.data());

    TEST_ASSERT(obj->avg(obj->copyCtorCount()) == 1, "Copy ctor should have been called once to initialize the buffer");
    TEST_ASSERT(obj->avg(obj->copyCount()) == 2, "Copy should have been called twice, once to and again from");
  }

  void Node_CopyCtorToAndFromBuffer_CtorCalledTwice(const TestObjFactory& factory) {
    std::unique_ptr<NodeTestObject> obj = factory();
    auto node = obj->getNode();
    std::vector<uint8_t> buffer(node->size());

    obj->reset();
    node->copyConstructToBuffer(obj.get(), buffer.data());
    node->copyConstructFromBuffer(obj.get(), buffer.data());
    TEST_ASSERT(obj->avg(obj->copyCtorCount()) == 2, "Copy ctor should be called once to copy in to buffer then again to copy back");
  }

  void Node_CopyCtorBufferToBuffer_CtorCalledTwice(const TestObjFactory& factory) {
    std::unique_ptr<NodeTestObject> obj = factory();
    auto node = obj->getNode();
    std::vector<uint8_t> source(node->size());
    std::vector<uint8_t> dest(node->size());

    obj->reset();
    node->copyConstructToBuffer(obj.get(), source.data());
    node->copyConstructBufferToBuffer(source.data(), dest.data());
    node->copyFromBuffer(obj.get(), dest.data());

    TEST_ASSERT(obj->avg(obj->copyCtorCount()) == 2, "Copy ctor should be called once to buffer, again from buffer to buffer");
    TEST_ASSERT(obj->avg(obj->copyCount()) == 1, "Copy should be called once for copying back in to obj");
  }

  void Node_CopyBufferToBuffer_CopyCalledOnce(const TestObjFactory& factory) {
    std::unique_ptr<NodeTestObject> obj = factory();
    auto node = obj->getNode();
    std::vector<uint8_t> source(node->size());
    std::vector<uint8_t> dest(node->size());

    obj->reset();
    node->copyConstructToBuffer(obj.get(), source.data());
    node->copyConstructToBuffer(obj.get(), dest.data());
    node->copyBufferToBuffer(source.data(), dest.data());
    node->copyConstructFromBuffer(obj.get(), dest.data());

    TEST_ASSERT(obj->avg(obj->copyCount()) == 1, "Copy should be called once for buffer to buffer");
    TEST_ASSERT(obj->avg(obj->copyCtorCount()) == 3, "Copy should be called once for each buffer and the copy back");
  }

  void Node_DestructBuffer_DtorCalled(const TestObjFactory& factory) {
    std::unique_ptr<NodeTestObject> obj = factory();
    auto node = obj->getNode();
    std::vector<uint8_t> buffer(node->size());

    obj->reset();
    node->copyConstructToBuffer(obj.get(), buffer.data());
    node->destructBuffer(buffer.data());

    TEST_ASSERT(obj->avg(obj->dtorCount()) == 1, "Dtor should have been called");
  }

  void Node_TestAllOnType(const TestObjFactory& factory) {
    Node_DefaultConstructAndDestroy_CtorAndDtorAreCalled(factory);
    Node_CopyToAndFromBuffer_CopyCalledTwice(factory);
    Node_CopyCtorToAndFromBuffer_CtorCalledTwice(factory);
    Node_CopyCtorBufferToBuffer_CtorCalledTwice(factory);
    Node_CopyBufferToBuffer_CopyCalledOnce(factory);
    Node_DestructBuffer_DtorCalled(factory);
  }

  template<typename NodeT>
  void Node_TestAll() {
    return Node_TestAllOnType([]() { return std::make_unique<NodeT>(); });
  }

  TEST_FUNC(Node_SingleValue) {
    Node_TestAll<SingleObj>();
  }

  TEST_FUNC(Node_DoubleValue) {
    Node_TestAll<DoubleObj>();
  }

  TEST_FUNC(Node_PaddedObject) {
    Node_TestAll<PaddedObj>();
  }

  TEST_FUNC(Node_ObjWithUnusedField) {
    Node_TestAll<UnusedFieldObj>();
  }

  TEST_FUNC(Node_UniqueValue) {
    Node_TestAll<UniquePtrObj>();
  }

  //TODO: write tests for these
  //size_t size() const;
  //NodeDiff getDiff(const void* base, const void* other) const;
  //void forEachDiff(NodeDiff diff, const void* base, const DiffCallback& callback) const;
  //void copyFromDiff(NodeDiff diff, const void* from, void* to) const;
}
