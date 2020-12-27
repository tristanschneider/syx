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
        typename WrappedNode::WrappedType value;
        //Read value, ignore key, since they're in order
        mWrapped._readFromLua(s, &value);
        vec.emplace_back(std::move(value));
        //Pop value
        lua_pop(s, 1);
      }
    }

    void _writeToLua(lua_State* s, const void* base) const override {
      const std::vector<WrappedNode::WrappedType>& vec = *static_cast<const std::vector<WrappedNode::WrappedType>*>(base);
      lua_createtable(s, vec.size(), 0);
      for(size_t i = 0; i < vec.size(); ++i) {
        auto&& obj = vec[i];
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
        typename KeyNode::WrappedType key;
        typename ValueNode::WrappedType value;
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
        const typename KeyNode::WrappedType& key = it.first;
        const typename ValueNode::WrappedType& value = it.second;
        mKeyNode._writeToLua(s, &key);
        mValueNode._writeToLua(s, &value);
        lua_settable(s, -3);
      }
    }

  protected:
    KeyNode mKeyNode;
    ValueNode mValueNode;
  };

  //A pointer node that can be used as a parent of other node and used to follow a pointer value in a structure
  //The pointer value must be non-null, as the node can't know how to create a new one
  template<typename Ptr>
  class PointerNode : public TypedNode<Ptr> {
  public:
    using TypedNode<Ptr>::TypedNode;
    using Base = TypedNode<Ptr>;
    //Follow pointer so children are relative to dereferenced address
    void _translateBase(const void*& base) const override {
      const auto& ptr = Base::_cast(base);
      if(ptr)
        base = &*ptr;
      else
        base = nullptr;
    }
    //These do nothing, children do the read instead, this is an invisible middle man
    void _readFromLua(lua_State*, void*) const override {}
    void _writeToLua(lua_State* s, const void*) const override {
      //Push the parent table so this still puts something on the stack, but doesn't create a new table
      lua_pushvalue(s, -1);
    }
    void getField(lua_State* s, Node::SourceType source = Node::SourceType::Default) const override {
      source;
      //Same as above, need something but not a new table
      lua_pushvalue(s, -1);
    }
    void setField(lua_State* s, Node::SourceType source = Node::SourceType::Default) const override {
      source;
      //Nothing to set since we didn't make a table, pop off the reference
      lua_pop(s, 1);
    }
    void _destruct(void* base) const override {
      auto& ptr = Base::_cast(base);
      ptr.~Ptr();
      //Not strictly necessary, although makes debugging less confusing if the pointers are null instead of garbage
      ptr = nullptr;
    }
  };
  template<typename T>
  class UniquePtrNode : public PointerNode<std::unique_ptr<T>> {
  public:
    using PointerNode<std::unique_ptr<T>>::PointerNode;
    using Base = TypedNode<std::unique_ptr<T>>;

    void _defaultConstruct(void* to) const override {
      new (to) std::unique_ptr<T>(std::make_unique<T>());
    }
    void _copyConstruct(const void* from, void* to) const override {
      new (to) std::unique_ptr<T>(std::make_unique<T>(*Base::_cast(from)));
    }
    void _copy(const void* from, void* to) const override {
      *Base::_cast(to) = *Base::_cast(from);
    }
    void _destruct(void* base) const override {
      Base::_cast(base).reset();
    }
  };

  class BufferNode : public TypedNode<std::vector<uint8_t>> {
  public:
    using TypedNode::TypedNode;
    using Base = TypedNode<std::vector<uint8_t>>;

    void destroyBuffer(std::vector<uint8_t>& buffer) const {
      if(!buffer.empty()) {
        for(const auto& child : mChildren)
          child->destruct(child->offset(buffer.data()));
      }
    }
    void _readFromLua(lua_State*, void* base) const override {
      bool needsConstruction = Base::_cast(base).empty();
      //Prepare buffer size in this _read so child _reads will have a place to read to
      Base::_cast(base).resize(size());
      if(needsConstruction) {
        Node::_translateBase(base);
        for(const auto& child : mChildren) {
          child->defaultConstruct(child->offset(base));
        }
      }
    }
    void _writeToLua(lua_State* s, const void*) const override {
      //Write table for children to fill
      lua_newtable(s);
    }
    void _defaultConstruct(void* to) const override {
      new (to) std::vector<uint8_t>(size());
    }
    void _translateBase(const void*& base) const override {
      base = _cast(base).data();
    }
  };
}
