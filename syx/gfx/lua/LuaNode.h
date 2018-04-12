#pragma once
//Nodes used to specify a structure to bind data to, allowing it to be
//read and written to lua. All nodes take a reference to the type to bind,
//which will be written or read. Overload makeNode for new node types.

struct lua_State;

namespace Syx {
  struct Vec3;
  struct Quat;
}

namespace Lua {
  class Node;

  struct NodeOps {
    NodeOps(Node& parent, std::string&& name, size_t offset);
    NodeOps(std::string&& name);
    NodeOps(const std::string& name);

    Node* mParent;
    std::string mName;
    size_t mOffset;
  };

  //read/write take base pointer so one scheme can be used between all instances of the class
  //members are then accessed through pointer offsets
  class Node {
  public:
    Node(NodeOps&& ops);
    virtual ~Node();
    Node(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(const Node&) = delete;

    //Read state from lua object(s) on stack or global
    void readFromLua(lua_State* s, void* base, bool fromGlobal = false) const;
    //Write state to new lua object(s) on stack or global
    void writeToLua(lua_State* s, const void* base, bool fromGlobal = false) const;

    //Attempt to read state from load object(s) on stack, returns if it was read
    bool readChildFromLua(lua_State* s, const char* child, void* base, bool fromGlobal = false) const;
    //Attempt to write state from load object(s) on stack, returns if it was read
    bool writeChildToLua(lua_State* s, const char* child, const void* base, bool fromGlobal = false) const;
    void addChild(std::unique_ptr<Node> child);

    const std::string& getName() const;

  protected:
    virtual void _readFromLua(lua_State* s, void* base) const {}
    virtual void _writeToLua(lua_State* s, const void* base) const {}

    //Push stack[top][field] onto top of stack, or global[field] if root node
    void getField(lua_State* s, const std::string& field, bool fromGlobal = false) const;
    //stack[top - 1][field] = stack[top]
    void setField(lua_State* s, const std::string& field, bool fromGlobal = false) const;
    //Same as above but uses mName
    void getField(lua_State* s, bool fromGlobal = false) const;
    void setField(lua_State* s, bool fromGlobal = false) const;

    NodeOps mOps;
    std::vector<std::unique_ptr<Node>> mChildren;
  };

  inline std::unique_ptr<Node> makeRootNode(NodeOps&& ops) {
    return std::make_unique<Node>(std::move(ops));
  }

  template<typename T>
  T& makeNode(NodeOps&& ops) {
    auto newNode = std::make_unique<T>(std::move(ops));
    auto& result = *newNode;
    ops.mParent->addChild(std::move(newNode));
    return result;
  }

  class IntNode : public Node {
  public:
    using WrappedType = int;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class StringNode : public Node {
  public:
    using WrappedType = std::string;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class FloatNode : public Node {
  public:
    using WrappedType = float;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class LightUserdataNode : public Node {
  public:
    using WrappedType = void*;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class LightUserdataSizetNode : public Node {
  public:
    using WrappedType = size_t;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class BoolNode : public Node {
  public:
    using WrappedType = bool;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class Vec3Node : public Node {
  public:
    using WrappedType = Syx::Vec3;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class QuatNode : public Node {
  public:
    using WrappedType = Syx::Quat;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };

  class Mat4Node : public Node {
  public:
    using WrappedType = Syx::Mat4;
    using Node::Node;
    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
  };
}
