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
  class State;
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
    void read(lua_State* s, uint8_t* base, bool fromGlobal = false) const;
    //Write state to new lua object(s) on stack or global
    void write(lua_State* s, uint8_t* base, bool fromGlobal = false) const;

    //Attempt to read state from load object(s) on stack, returns if it was read
    bool readChild(lua_State* s, const char* child, uint8_t* base, bool fromGlobal = false) const;
    //Attempt to write state from load object(s) on stack, returns if it was read
    bool writeChild(lua_State* s, const char* child, uint8_t* base, bool fromGlobal = false) const;
    void addChild(std::unique_ptr<Node> child);

    const std::string& getName() const;

  protected:
    virtual void _read(lua_State* s, uint8_t* base) const {}
    virtual void _write(lua_State* s, uint8_t* base) const {}

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
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    int& _get(uint8_t* base) const;
  };

  class StringNode : public Node {
  public:
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    std::string& _get(uint8_t* base) const;
  };

  class FloatNode : public Node {
  public:
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    float& _get(uint8_t* base) const;
  };

  class LightUserdataNode : public Node {
  public:
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    void*& _get(uint8_t* base) const;
  };

  class LightUserdataSizetNode : public Node {
  public:
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    size_t& _get(uint8_t* base) const;
  };

  class BoolNode : public Node {
  public:
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    bool& _get(uint8_t* base) const;
  };

  class Vec3Node : public Node {
  public:
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    Syx::Vec3& _get(uint8_t* base) const;
  };

  class QuatNode : public Node {
  public:
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    Syx::Quat& _get(uint8_t* base) const;
  };

  class Mat4Node : public Node {
  public:
    using Node::Node;
    void _read(lua_State* s, uint8_t* base) const override;
    void _write(lua_State* s, uint8_t* base) const override;

  protected:
    Syx::Mat4& _get(uint8_t* base) const;
  };
}
