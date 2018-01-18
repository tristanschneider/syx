#include "Precompile.h"
#include "lua/LuaNode.h"
#include "lua/LuaState.h"

#include <lua.hpp>

namespace Lua {
  NodeOps::NodeOps(Node& parent, std::string&& name)
    : mParent(&parent)
    , mName(std::move(name)) {
  }

  NodeOps::NodeOps(std::string&& name)
    : mParent(nullptr)
    , mName(std::move(name)) {
  }

  Node::Node(NodeOps&& ops)
    : mOps(std::move(ops)) {
  }

  Node::~Node() {
  }

  void Node::addChild(std::unique_ptr<Node> child) {
    child->mOps.mParent = this;
    mChildren.emplace_back(std::move(child));
  }

  void Node::getField(State& s, const std::string& field) const {
    if(!mOps.mParent)
      lua_getglobal(s, field.c_str());
    else
      lua_getfield(s, -1, field.c_str());
  }

  void Node::setField(State& s, const std::string& field) const {
    if(!mOps.mParent)
      lua_setglobal(s, field.c_str());
    else
      lua_setfield(s, -2, field.c_str());
  }

  void Node::getField(State& s) const {
    getField(s, mOps.mName);
  }

  void Node::setField(State& s) const {
    setField(s, mOps.mName);
  }

  void Node::read(State& s) {
    getField(s);
    _read(s);
    for(auto& child : mChildren)
      child->read(s);
    lua_pop(s, 1);
  }

  void Node::write(State& s) const {
    if(mChildren.size()) {
      //Make new table and allow children to fill it
      lua_newtable(s);
      for(auto& child : mChildren)
        child->write(s);
      //Store new table on parent
      setField(s);
    }
    else {
      //Leaf node, write to field in parent's table
      _write(s);
    }
  }

  IntNode::IntNode(NodeOps&& ops, int& i)
    : Node(std::move(ops))
    , mI(&i) {
  }

  void IntNode::_read(State& s) {
    *mI = static_cast<int>(lua_tointeger(s, -1));
  }

  void IntNode::_write(State& s) const {
    lua_pushinteger(s, *mI);
    setField(s);
  }

  StringNode::StringNode(NodeOps&& ops, std::string& str)
    : Node(std::move(ops))
    , mStr(&str) {
  }

  void StringNode::_read(State& s) {
    *mStr = lua_tostring(s, -1);
  }

  void StringNode::_write(State& s) const {
    lua_pushlstring(s, mStr->c_str(), mStr->size());
    setField(s);
  }
}