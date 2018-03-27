#include "Precompile.h"
#include "lua/LuaNode.h"
#include "lua/LuaState.h"

#include <lua.hpp>
#include <SyxVec3.h>
#include <SyxQuat.h>

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

  void Node::getField(State& s, const std::string& field, bool fromGlobal) const {
    if(!mOps.mParent) {
      if(fromGlobal)
        lua_getglobal(s, field.c_str());
      //else value is expected to already be on top of the stack
    }
    else
      lua_getfield(s, -1, field.c_str());
  }

  void Node::setField(State& s, const std::string& field, bool fromGlobal) const {
    if(!mOps.mParent) {
      if(fromGlobal)
        lua_setglobal(s, field.c_str());
      //else value is left on top of stack
    }
    else
      lua_setfield(s, -2, field.c_str());
  }

  void Node::getField(State& s, bool fromGlobal) const {
    getField(s, mOps.mName, fromGlobal);
  }

  void Node::setField(State& s, bool fromGlobal) const {
    setField(s, mOps.mName, fromGlobal);
  }

  void Node::read(State& s, uint8_t* base, bool fromGlobal) const {
    getField(s);
    _read(s, base);
    for(auto& child : mChildren)
      child->read(s, base + child->mOps.mOffset);
    lua_pop(s, 1);
  }

  void Node::write(State& s, uint8_t* base, bool fromGlobal) const {
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

  bool Node::readChild(State& s, const char* child, uint8_t* base, bool fromGlobal) const {
    for(const auto& c : mChildren) {
      if(!strcmp(child, c->getName().c_str())) {
        getField(s, fromGlobal);
        _read(s, base);
        c->read(s, base + c->mOps.mOffset, false);
        lua_pop(s, 1);
        return true;
      }
    }
    return false;
  }

  bool Node::writeChild(State& s, const char* child, uint8_t* base, bool fromGlobal) const {
    for(const auto& c : mChildren) {
      if(!strcmp(child, c->getName().c_str())) {
        lua_newtable(s);
        c->write(s, base + c->mOps.mOffset, false);
        setField(s, fromGlobal);
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

  void FloatNode::_read(State& s, uint8_t* base) const {
    _get(base) = static_cast<float>(lua_tonumber(s, -1));
  }

  void FloatNode::_write(State& s, uint8_t* base) const {
    lua_pushnumber(s, _get(base));
    setField(s);
  }

  float& FloatNode::_get(uint8_t* base) const {
    return *reinterpret_cast<float*>(base);
  }

  void LightUserdataNode::_read(State& s, uint8_t* base) const {
    _get(base) = lua_touserdata(s, -1);
  }

  void LightUserdataNode::_write(State& s, uint8_t* base) const {
    lua_pushlightuserdata(s, _get(base));
    setField(s);
  }

  void*& LightUserdataNode::_get(uint8_t* base) const {
    return *reinterpret_cast<void**>(base);
  }

  void BoolNode::_read(State& s, uint8_t* base) const {
    _get(base) = static_cast<bool>(lua_toboolean(s, -1));
  }

  void BoolNode::_write(State& s, uint8_t* base) const {
    lua_pushboolean(s, static_cast<int>(_get(base)));
    setField(s);
  }

  bool& BoolNode::_get(uint8_t* base) const {
    return *reinterpret_cast<bool*>(base);
  }

  void Vec3Node::_read(State& s, uint8_t* base) const {
    Syx::Vec3& v = _get(base);
    for(int i = 0; i < 3; ++i) {
      lua_pushinteger(s, i + 1);
      lua_gettable(s, -2);
      v[i] = static_cast<float>(lua_tonumber(s, -1));
      lua_pop(s, 1);
    }
  }

  void Vec3Node::_write(State& s, uint8_t* base) const {
    lua_createtable(s, 3, 0);
    Syx::Vec3& v = _get(base);
    for(int i = 0; i < 3; ++i) {
      lua_pushinteger(s, i + 1);
      lua_pushnumber(s, v[i]);
      lua_settable(s, -3);
    }
    setField(s);
  }

  Syx::Vec3& Vec3Node::_get(uint8_t* base) const {
    return *reinterpret_cast<Syx::Vec3*>(base);
  }

  void QuatNode::_read(State& s, uint8_t* base) const {
    Syx::Quat& v = _get(base);
    for(int i = 0; i < 4; ++i) {
      lua_pushinteger(s, i + 1);
      lua_gettable(s, -2);
      v[i] = static_cast<float>(lua_tonumber(s, -1));
      lua_pop(s, 1);
    }
  }

  void QuatNode::_write(State& s, uint8_t* base) const {
    lua_createtable(s, 3, 0);
    Syx::Quat& v = _get(base);
    for(int i = 0; i < 4; ++i) {
      lua_pushinteger(s, i + 1);
      lua_pushnumber(s, v[i]);
      lua_settable(s, -3);
    }
    setField(s);
  }

  Syx::Quat& QuatNode::_get(uint8_t* base) const {
    return *reinterpret_cast<Syx::Quat*>(base);
  }
}