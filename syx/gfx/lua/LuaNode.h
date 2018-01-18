#pragma once
//Nodes used to specify a structure to bind data to, allowing it to be
//read and written to lua. All nodes take a reference to the type to bind,
//which will be written or read. Overload makeNode for new node types.

namespace Lua {
  class State;
  class Node;

  #define MAKE_NODE(NodeType, ParamType)\
    inline std::unique_ptr<NodeType> makeRootNode(NodeOps&& ops, ParamType& p) {\
      return std::make_unique<NodeType>(std::move(ops), p);\
    }\
    inline NodeType& makeNode(NodeOps&& ops, ParamType& p) {\
      Node* parent = ops.mParent;\
      auto newNode = std::make_unique<NodeType>(std::move(ops), p);\
      auto& result = *newNode;\
      parent->addChild(std::move(newNode));\
      return result;\
    }

  struct NodeOps {
    NodeOps(Node& parent, std::string&& name);
    NodeOps(std::string&& name);

    Node* mParent;
    std::string mName;
  };

  class Node {
  public:
    Node(NodeOps&& ops);
    virtual ~Node();
    Node(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(const Node&) = delete;

    void read(State& s);
    void write(State& s) const;

    void addChild(std::unique_ptr<Node> child);
  protected:
    virtual void _read(State& s) {}
    virtual void _write(State& s) const {}

    //Push stack[top][field] onto top of stack, or global[field] if root node
    void getField(State& s, const std::string& field) const;
    //stack[top - 1][field] = stack[top]
    void setField(State& s, const std::string& field) const;
    //Same as above but uses mName
    void getField(State& s) const;
    void setField(State& s) const;

    NodeOps mOps;
    std::vector<std::unique_ptr<Node>> mChildren;
  };

  inline std::unique_ptr<Node> makeRootNode(NodeOps&& ops) {
    return std::make_unique<Node>(std::move(ops));
  }

  inline Node& makeNode(NodeOps&& ops) {
    auto newNode = std::make_unique<Node>(std::move(ops));
    auto& result = *newNode;
    ops.mParent->addChild(std::move(newNode));
    return result;
  }

  class IntNode : public Node {
  public:
    IntNode(NodeOps&& ops, int& i);
    void _read(State& s) override;
    void _write(State& s) const override;

  protected:
    int* mI;
  };
  MAKE_NODE(IntNode, int);

  class StringNode : public Node {
  public:
    StringNode(NodeOps&& ops, std::string& str);
    void _read(State& s) override;
    void _write(State& s) const override;

  protected:
    std::string* mStr;
  };
  MAKE_NODE(StringNode, std::string);
}
