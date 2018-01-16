#pragma once
//Nodes used to specify a structure to bind data to, allowing it to be
//read and written to lua. All nodes take a reference to the type to bind,
//which will be written or read. Overload makeNode for new node types.

namespace Lua {
  class State;

  #define MAKE_NODE(NodeType, ParamType)\
    inline std::unique_ptr<NodeType> makeRootNode(const std::string& name, ParamType& p) {\
      return std::make_unique<NodeType>(nullptr, name, p);\
    }\
    inline NodeType& makeNode(Node& parent, const std::string& name, ParamType& p) {\
      auto newNode = std::make_unique<NodeType>(&parent, name, p);\
      auto& result = *newNode;\
      parent.addChild(std::move(newNode));\
      return result;\
    }

  class Node {
  public:
    Node(Node* parent, const std::string& name);
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

    Node* mParent;
    std::vector<std::unique_ptr<Node>> mChildren;
    std::string mName;
  };

  inline std::unique_ptr<Node> makeRootNode(const std::string& name) {
    return std::make_unique<Node>(nullptr, name);
  }

  inline Node& makeNode(Node& parent, const std::string& name) {
    auto newNode = std::make_unique<Node>(&parent, name);
    auto& result = *newNode;
    parent.addChild(std::move(newNode));
    return result;
  }

  class IntNode : public Node {
  public:
    IntNode(Node* parent, const std::string& name, int& i);
    void _read(State& s) override;
    void _write(State& s) const override;

  protected:
    int& mI;
  };
  MAKE_NODE(IntNode, int);
}
