#pragma once
//Nodes used to specify a structure to bind data to, allowing it to be
//read and written to lua. All nodes take a reference to the type to bind,
//which will be written or read. Overload makeNode for new node types.
//This class has two main purposes, which cause some problematic behavior.
//The first purpose is to be able to read to and write a class hierarchy to lua. This works fine.
//The second is to allow the object to be copied to a buffer safely, and restored, without statically
//having to know what the type is.
//This mostly works for plain types, but is tricky for dynamic ones. The size of the node is expected to be static,
//so the current approach is to placement construct the dynamic class into the buffer. This means the size is still static,
//since the buffer will contain an object with a pointer to a buffer it allocated for itself. It will be freed when the user calls the destructors
//on everything in the buffer. That in itself is also not so great since that's something that the developer could forget to do.

struct lua_State;

#include "SyxMat4.h"
#include "SyxVec3.h"
#include "SyxQuat.h"

namespace Lua {
  class Node;

  //Bitfield where bits set indicate the object at that traversal order (depth first) is different
  using NodeDiff = uint64_t;

  struct NodeOps {
    NodeOps(Node& parent, std::string&& name, size_t offset);
    NodeOps(Node& parent, int index, size_t offset);
    NodeOps(std::string&& name);
    NodeOps(const std::string& name);

    void pushKey(lua_State* s) const;

    Node* mParent;
    std::string mName;
    //Index value for key if it is not a string
    int mIndex;
    size_t mOffset;

  private:
    NodeOps(Node* parent, std::string&& name, int index, size_t offset);
  };

  //read/write take base pointer so one scheme can be used between all instances of the class
  //members are then accessed through pointer offsets
  class Node {
  public:
    using DiffCallback = std::function<void(const Node&, const void*)>;

    enum class SourceType {
      FromGlobal,
      FromStack,
      FromParent,
      Default = FromParent
    };
    Node(NodeOps&& ops);
    virtual ~Node();
    Node(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(const Node&) = delete;

    //Read state from lua object(s) on stack or global
    bool readFromLua(lua_State* s, void* base, SourceType source = SourceType::Default) const;
    //Write state to new lua object(s) on stack or global
    void writeToLua(lua_State* s, const void* base, SourceType source = SourceType::Default) const;
    //Read state from lua object(s) on stack or global into flat buffer. Values are default constructed into buffer then assigned. Caller must ensure buffer has size() bytes
    bool readFromLuaToBuffer(lua_State* s, void* buffer, SourceType source = SourceType::Default) const;

    const Node* getChild(const char* child) const;
    void addChild(std::unique_ptr<Node> child);
    void forEachChildShallow(std::function<void(const Node&)> callback) const;

    //Total size in bytes of tree
    size_t size() const;
    //Copy construct each node in tree into a flatttened buffer. Caller must ensure buffer has at least size() bytes left
    void copyConstructToBuffer(const void* base, void* buffer) const;
    void copyConstructFromBuffer(void* base, const void* buffer, NodeDiff diff = ~0) const;
    //Copy each node in tree. Caller must ensure buffer has at least size() bytes left
    void copyToBuffer(const void* base, void* buffer) const;
    void copyFromBuffer(void* base, const void* buffer, NodeDiff diff = ~0) const;
    //Copy construct each value in buffer to new location
    void copyConstructBufferToBuffer(const void* from, void* to) const;
    void copyBufferToBuffer(const void* from, void* to) const;
    //Call destructor on each value in buffer
    void destructBuffer(void* buffer) const;

    void defaultConstruct(void* base) const;
    void destruct(void* base) const;

    //Get typeid of the wrapped type of this node. void if it doesn't wrap a type
    virtual size_t getTypeId() const;

    NodeDiff getDiff(const void* base, const void* other) const;
    //Iterate over all nodes on the object, which should already be constructed
    void forEachDiff(NodeDiff diff, const void* base, const DiffCallback& callback) const;

    //Translate base which is the root of the tree down to this node
    const void* _translateBaseToNode(const void* base) const;
    void* _translateBaseToNode(void* base) const;
    const void* _translateBufferToNode(const void* buffer) const;
    void* _translateBufferToNode(void* buffer) const;
    //Traverse hierarchy to find the diff id of this node
    NodeDiff _getDiffId() const;

    //Size of this node in bytes
    virtual size_t _size() const { return 0; }
    //Default construct node at location
    virtual void _defaultConstruct(void* to) const { to; }
    //Copy construct node
    virtual void _copyConstruct(const void* from, void* to) const { from; to; }
    //Copy node
    virtual void _copy(const void* from, void* to) const { from; to; }
    //Call destructor on node
    virtual void _destruct(void*) const {}
    //Equality, used for generating diff
    virtual bool _equals(const void*, const void*) const { return true; }
    //True if the wrapped type owns child memory, meaning destroying this would automatically destroy children without needing an explicit _destruct call
    //Overriding destruct should mean that this is true
    virtual bool _ownsChildMemory() const { return true; }

    const void* offset(const void* base) const;
    void* offset(void* base) const;

    const std::string& getName() const;

    bool hasInspector() const;
    bool inspect(void* prop) const;
    Node& setInspector(std::function<bool(const char*, void*)> inspector);

  protected:
    virtual void _readFromLua(lua_State* s, void* base) const = 0;
    virtual void _writeToLua(lua_State* s, const void* base) const = 0;

    //Traverse the tree and use func to construct/move/copy to/from buffer
    void _funcToBuffer(void (Node::* func)(const void*, void*) const, const void* base, void* buffer) const;
    void _funcFromBuffer(void (Node::* func)(const void*, void*) const, void* base, const void* buffer, NodeDiff diff) const;
    void _funcFromBuffer(void (Node::* func)(const void*, void*) const, void* base, const void* buffer, NodeDiff diff, int& nodeIndex) const;
    //Traverse buffer and use func on each leaf node
    void _funcBufferToBuffer(void (Node::* func)(const void*, void*) const, const void* from, void* to) const;
    void _forEachBottomUp(void (Node::* func)(void*) const, void* base) const;
    NodeDiff _getDiff(const void* base, const void* other, int& nodeIndex) const;
    void _forEachDiff(NodeDiff diff, const void* base, const DiffCallback& callback, int& nodeIndex) const;

    void _forEachDepthFirstToChild(void (Node::* func)(const Node&, void*) const, void* data) const;
    void _countNodes(const Node& node, void* data) const;
    void _countNodeSizes(const Node& node, void* data) const;

    //Translate the location of base based on this node type. Usually nothing but can be used to follow pointers
    virtual void _translateBase(const void*&) const {}
    void _translateBase(void*& base) const { _translateBase(const_cast<const void*&>(base)); }

    //Push stack[top][field] onto top of stack, or global[field] if root node
    virtual void getField(lua_State* s, SourceType source = SourceType::Default) const;
    //stack[top - 1][field] = stack[top]
    virtual void setField(lua_State* s, SourceType source = SourceType::Default) const;

    NodeOps mOps;
    std::vector<std::unique_ptr<Node>> mChildren;
    //Size of tree from here down
    size_t mSize;
    std::function<bool(const char*, void*)> mInspector;
  };

  class RootNode : public Node {
  public:
    static void _writeRootToLua(lua_State* s);

    using Node::Node;
    void _readFromLua(lua_State*, void*) const override {}
    void _writeToLua(lua_State* s, const void* base) const override;
    bool _ownsChildMemory() const override { return false; }
  };

  inline std::unique_ptr<Node> makeRootNode(NodeOps&& ops) {
    return std::make_unique<RootNode>(std::move(ops));
  }

  template<typename T, typename... Args>
  T& makeNode(NodeOps&& ops, Args&&... args) {
    auto newNode = std::make_unique<T>(std::move(ops), std::forward(args)...);
    auto& result = *newNode;
    ops.mParent->addChild(std::move(newNode));
    return result;
  }

  template<typename T>
  class TypedNode : public Node {
  public:
    using Node::Node;
    using WrappedType = T;
  protected:
    size_t _size() const override {
      return sizeof(T);
    }
    void _defaultConstruct(void* to) const override {
      new (to) T();
    }
    void _copyConstruct(const void* from, void* to) const override {
      _copyConstructIfCopyable<T>(from, to);
    }
    void _copy(const void* from, void* to) const override {
      _copyIfCopyable<T>(from, to);
    }
    void _destruct(void* base) const override {
      _cast(base).~T();
    }
    bool _equals(const void* lhs, const void* rhs) const override {
      return _cast(lhs) == _cast(rhs);
    }
    size_t getTypeId() const override {
      return typeId<T>();
    }
    T& _cast(void* value) const {
      return *static_cast<T*>(value);
    }
    const T& _cast(const void* value) const {
      return *static_cast<const T*>(value);
    }

    template<typename S>
    std::enable_if_t<std::is_copy_assignable_v<S>> _copyIfCopyable(const void* from, void* to) const {
      _cast(to) = _cast(from);
    }
    template<typename S>
    std::enable_if_t<!std::is_copy_assignable_v<S>> _copyIfCopyable(const void*, void*) const {
      assert(false && "Wrapped type isn't copyable, node should override _copy");
    }
    template<typename S>
    std::enable_if_t<std::is_copy_constructible<S>::value> _copyConstructIfCopyable(const void* from, void* to) const {
      new (to) T(_cast(from));
    }
    template<typename S>
    std::enable_if_t<!std::is_copy_constructible<S>::value> _copyConstructIfCopyable(const void*, void*) const {
      assert(false && "Wrapped type isn't copyable, node should override _copyConstruct");
    }
  };

  //Be careful when using this not to have any non-owning pointers in the class, as they will be copied by value into buffers and potentially unsafe
  template<class T>
  struct OwnerRootNode : public TypedNode<T> {
    using TypedNode<T>::TypedNode;
    void _readFromLua(lua_State*, void*) const override {}
    void _writeToLua(lua_State* s, const void*) const override {
      RootNode::_writeRootToLua(s);
    }
    bool _ownsChildMemory() const override { return true; }
  };

#define NODE_SINGLETON(NodeType) static const NodeType& singleton() { static NodeType s(NodeOps("")); return s; }

  class IntNode : public TypedNode<int> {
  public:
    NODE_SINGLETON(IntNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class StringNode : public TypedNode<std::string> {
  public:
    NODE_SINGLETON(StringNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class FloatNode : public TypedNode<float> {
  public:
    NODE_SINGLETON(FloatNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class LightUserdataNode : public TypedNode<void*> {
  public:
    NODE_SINGLETON(LightUserdataNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class LightUserdataSizetNode : public TypedNode<size_t> {
  public:
    NODE_SINGLETON(LightUserdataSizetNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class BoolNode : public TypedNode<bool> {
  public:
    NODE_SINGLETON(BoolNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class SizetNode : public TypedNode<size_t> {
  public:
    NODE_SINGLETON(SizetNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class Uint64Node : public TypedNode<uint64_t> {
  public:
    NODE_SINGLETON(Uint64Node);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class DoubleNode : public TypedNode<double> {
  public:
    NODE_SINGLETON(DoubleNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class Vec3Node : public TypedNode<Syx::Vec3> {
  public:
    NODE_SINGLETON(Vec3Node);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class QuatNode : public TypedNode<Syx::Quat> {
  public:
    NODE_SINGLETON(QuatNode);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class Mat4Node : public TypedNode<Syx::Mat4> {
  public:
    NODE_SINGLETON(Mat4Node);
    using TypedNode::TypedNode;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };
}
