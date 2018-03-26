#include "Precompile.h"
#include "lua/LuaNode.h"
#include "lua/LuaState.h"

#include <lua.hpp>

namespace Lua {
  NodeOps::NodeOps(Node& parent, std::string&& name, size_t offset)
    : mParent(&parent)
    , mName(std::move(name))
    , mOffset(offset) {
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
    if(mOps.mParent)
      lua_getfield(s, -1, field.c_str());
  }

  void Node::setField(State& s, const std::string& field) const {
    if(mOps.mParent)
      lua_setfield(s, -2, field.c_str());
  }

  void Node::getField(State& s) const {
    getField(s, mOps.mName);
  }

  void Node::setField(State& s) const {
    setField(s, mOps.mName);
  }

  void Node::read(State& s, uint8_t* base) const {
    getField(s);
    _read(s, base);
    for(auto& child : mChildren)
      child->read(s, base + child->mOps.mOffset);
    lua_pop(s, 1);
  }

  void Node::write(State& s, uint8_t* base) const {
    if(mChildren.size()) {
      //Make new table and allow children to fill it
      lua_newtable(s);
      for(auto& child : mChildren)
        child->write(s, base + child->mOps.mOffset);
      //Store new table on parent
      setField(s);
    }
    else {
      //Leaf node, write to field in parent's table
      _write(s, base);
    }
  }

  bool Node::readChild(State& s, const char* child, uint8_t* base) const {
    for(const auto& c : mChildren) {
      if(!strcmp(child, c->getName().c_str())) {
        getField(s);
        _read(s, base);
        c->read(s, base + c->mOps.mOffset);
        lua_pop(s, 1);
        return true;
      }
    }
    return false;
  }

  bool Node::writeChild(State& s, const char* child, uint8_t* base) const {
    for(const auto& c : mChildren) {
      if(!strcmp(child, c->getName().c_str())) {
        lua_newtable(s);
        c->write(s, base + c->mOps.mOffset);
        setField(s);
        return true;
      }
    }
    return false;
  }

  const std::string& Node::getName() const {
    return mOps.mName;
  }

  void IntNode::_read(State& s, uint8_t* base) const {
    _get(base) = static_cast<int>(lua_tointeger(s, -1));
  }

  void IntNode::_write(State& s, uint8_t* base) const {
    lua_pushinteger(s, _get(base));
    setField(s);
  }

  int& IntNode::_get(uint8_t* base) const {
    return *reinterpret_cast<int*>(base);
  }

  void StringNode::_read(State& s, uint8_t* base) const {
    _get(base) = lua_tostring(s, -1);
  }

  void StringNode::_write(State& s, uint8_t* base) const {
    lua_pushlstring(s, _get(base).c_str(), _get(base).size());
    setField(s);
  }

  std::string& StringNode::_get(uint8_t* base) const {
    return *reinterpret_cast<std::string*>(base);
  }
}