#include "Precompile.h"
#include "lua/LuaNode.h"
#include "lua/LuaState.h"
#include "lua/lib/LuaVec3.h"
#include "lua/lib/LuaQuat.h"

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

  NodeOps::NodeOps(const std::string& name)
    : mParent(nullptr)
    , mName(name) {
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

  void Node::getField(lua_State* s, const std::string& field, bool fromGlobal) const {
    if(!mOps.mParent) {
      if(fromGlobal)
        lua_getglobal(s, field.c_str());
      //else value is expected to already be on top of the stack
    }
    else
      lua_getfield(s, -1, field.c_str());
  }

  void Node::setField(lua_State* s, const std::string& field, bool fromGlobal) const {
    if(!mOps.mParent) {
      if(fromGlobal)
        lua_setglobal(s, field.c_str());
      //else value is left on top of stack
    }
    else
      lua_setfield(s, -2, field.c_str());
  }

  void Node::getField(lua_State* s, bool fromGlobal) const {
    getField(s, mOps.mName, fromGlobal);
  }

  void Node::setField(lua_State* s, bool fromGlobal) const {
    setField(s, mOps.mName, fromGlobal);
  }

  void Node::readFromLua(lua_State* s, void* base, bool fromGlobal) const {
    getField(s);
    _readFromLua(s, base);
    for(auto& child : mChildren)
      child->readFromLua(s, Util::offset(base, child->mOps.mOffset));
    lua_pop(s, 1);
  }

  void Node::writeToLua(lua_State* s, const void* base, bool fromGlobal) const {
    if(mChildren.size()) {
      //Make new table and allow children to fill it
      lua_newtable(s);
      for(auto& child : mChildren) {
        child->writeToLua(s, Util::offset(base, + child->mOps.mOffset));
        child->setField(s);
      }
      //Store new table on parent
      setField(s);
    }
    else {
      //Leaf node, write to field in parent's table
      _writeToLua(s, base);
    }
  }

  bool Node::readChildFromLua(lua_State* s, const char* child, void* base, bool fromGlobal) const {
    for(const auto& c : mChildren) {
      if(!strcmp(child, c->getName().c_str())) {
        getField(s, fromGlobal);
        _readFromLua(s, base);
        c->readFromLua(s, Util::offset(base, c->mOps.mOffset), false);
        lua_pop(s, 1);
        return true;
      }
    }
    return false;
  }

  bool Node::writeChildToLua(lua_State* s, const char* child, const void* base, bool fromGlobal) const {
    for(const auto& c : mChildren) {
      if(!strcmp(child, c->getName().c_str())) {
        lua_newtable(s);
        c->writeToLua(s, Util::offset(base, c->mOps.mOffset), false);
        setField(s, fromGlobal);
        return true;
      }
    }
    return false;
  }

  const std::string& Node::getName() const {
    return mOps.mName;
  }

  void IntNode::_readFromLua(lua_State* s, void* base) const {
    *static_cast<int*>(base) = static_cast<int>(lua_tointeger(s, -1));
  }

  void IntNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushinteger(s, *static_cast<const int*>(base));
  }

  void StringNode::_readFromLua(lua_State* s, void* base) const {
    *static_cast<std::string*>(base) = lua_tostring(s, -1);
  }

  void StringNode::_writeToLua(lua_State* s, const void* base) const {
    const std::string& str = *static_cast<const std::string*>(base);
    lua_pushlstring(s, str.c_str(), str.size());
  }

  void FloatNode::_readFromLua(lua_State* s, void* base) const {
    *static_cast<float*>(base) = static_cast<float>(lua_tonumber(s, -1));
  }

  void FloatNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushnumber(s, *static_cast<const float*>(base));
  }

  void LightUserdataNode::_readFromLua(lua_State* s, void* base) const {
    *static_cast<void**>(base) = lua_touserdata(s, -1);
  }

  void LightUserdataNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushlightuserdata(s, *static_cast<void**>(const_cast<void*>(base)));
  }

  void LightUserdataSizetNode::_readFromLua(lua_State* s, void* base) const {
    *static_cast<size_t*>(base) = reinterpret_cast<size_t>(lua_touserdata(s, -1));
  }

  void LightUserdataSizetNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushlightuserdata(s, &(const_cast<size_t&>(*static_cast<const size_t*>(base))));
  }

  void BoolNode::_readFromLua(lua_State* s, void* base) const {
    *static_cast<bool*>(base) = static_cast<bool>(lua_toboolean(s, -1));
  }

  void BoolNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushboolean(s, static_cast<int>(*static_cast<const bool*>(base)));
  }

  void Vec3Node::_readFromLua(lua_State* s, void* base) const {
    *static_cast<Syx::Vec3*>(base) = Lua::Vec3::_getVec(s, -1);
  }

  void Vec3Node::_writeToLua(lua_State* s, const void* base) const {
    Lua::Vec3::construct(s, *static_cast<const Syx::Vec3*>(base));
  }

  void QuatNode::_readFromLua(lua_State* s, void* base) const {
    *static_cast<Syx::Quat*>(base) = Lua::Quat::_getQuat(s, -1);
  }

  void QuatNode::_writeToLua(lua_State* s, const void* base) const {
    Lua::Quat::construct(s, *static_cast<const Syx::Quat*>(base));
  }

  void Mat4Node::_readFromLua(lua_State* s, void* base) const {
    Syx::Mat4& m = *static_cast<Syx::Mat4*>(base);
    for(int i = 0; i < 16; ++i) {
      lua_pushinteger(s, i + 1);
      lua_gettable(s, -2);
      m.mData[i] = static_cast<float>(lua_tonumber(s, -1));
      lua_pop(s, 1);
    }
  }

  void Mat4Node::_writeToLua(lua_State* s, const void* base) const {
    lua_createtable(s, 16, 0);
    const Syx::Mat4& m = *static_cast<const Syx::Mat4*>(base);
    for(int i = 0; i < 16; ++i) {
      lua_pushinteger(s, i + 1);
      lua_pushnumber(s, m.mData[i]);
      lua_settable(s, -3);
    }
    setField(s);
  }
}