#pragma once
//Nodes used to specify a structure to bind data to, allowing it to be
//read and written to lua. All nodes take a reference to the type to bind,
//which will be written or read. Overload makeNode for new node types.

#include <lua.hpp>
#include "lua/LuaNode.h"

struct lua_State;

namespace Syx {
  struct Vec3;
  struct Quat;
}

namespace Lua {
  class Node;

  template<typename WrappedNode>
  class VectorNode : public Node {
  public:
    VectorNode(NodeOps&& ops)
      : Node(std::move(ops))
      , mWrapped(NodeOps("")) {
    }

    void _readFromLua(lua_State* s, void* base) const override {
      std::vector<WrappedNode::WrappedType>& vec = *static_cast<std::vector<WrappedNode::WrappedType>*>(base);
      vec.clear();
      //Dummy value for next to pop off
      lua_pushnil(s);
      while(lua_next(s, -2)) {
        //Key is now at -2 and value at -1
        WrappedNode::WrappedType value;
        //Read value, ignore key, since they're in order
        mWrapped._readFromLua(s, &value);
        vec.push_back(value);
        //Pop value
        lua_pop(s, 1);
      }
    }

    void _writeToLua(lua_State* s, const void* base) const override {
      const std::vector<WrappedNode::WrappedType>& vec = *static_cast<const std::vector<WrappedNode::WrappedType>*>(base);
      lua_createtable(s, vec.size(), 0);
      for(size_t i = 0; i < vec.size(); ++i) {
        WrappedNode::WrappedType&& obj = vec[i];
        lua_pushinteger(s, static_cast<lua_Integer>(i + 1));
        mWrapped._writeToLua(s, &obj);
        lua_settable(s, -3);
      }
    }

  protected:
    WrappedNode mWrapped;
  };

  template<typename KeyNode, typename ValueNode>
  class UnorderedMapNode : public Node {
  public:
    UnorderedMapNode(NodeOps&& ops)
      : Node(std::move(ops))
      , mKeyNode(NodeOps(""))
      , mValueNode(NodeOps("")) {
    }

    void _readFromLua(lua_State* s, void* base) const override {
      auto& map = *static_cast<std::unordered_map<KeyNode::WrappedType, ValueNode::WrappedType>*>(base);
      map.clear();
      //Dummy value for next to pop off
      lua_pushnil(s);
      while(lua_next(s, -2)) {
        //Key is now at -2 and value at -1
        KeyNode::WrappedType key;
        ValueNode::WrappedType value;
        mValueNode._readFromLua(s, &value);
        //Pop value, leaving key on top to read
        lua_pop(s, 1);
        mKeyNode._readFromLua(s, &key);
        map[key] = value;
      }
    }

    void _writeToLua(lua_State* s, const void* base) const override {
      const auto& map = *static_cast<const std::unordered_map<KeyNode::WrappedType, ValueNode::WrappedType>*>(base);
      lua_createtable(s, map.size(), 0);
      for(const auto& it : map) {
        const KeyNode::WrappedType& key = it.first;
        const ValueNode::WrappedType& value = it.second;
        mKeyNode._writeToLua(s, &key);
        mValueNode._writeToLua(s, &value);
        lua_settable(s, -3);
      }
    }

  protected:
    KeyNode mKeyNode;
    ValueNode mValueNode;
  };
}
