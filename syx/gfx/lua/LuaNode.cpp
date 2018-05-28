#include "Precompile.h"
#include "lua/LuaNode.h"

#include "allocator/LIFOAllocator.h"
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
    , mName(std::move(name))
    , mOffset(0) {
  }

  NodeOps::NodeOps(const std::string& name)
    : mParent(nullptr)
    , mName(name)
    , mOffset(0) {
  }

  Node::Node(NodeOps&& ops)
    : mOps(std::move(ops))
    , mSize(0) {
  }

  Node::~Node() {
  }

  void Node::addChild(std::unique_ptr<Node> child) {
    Node* parent = this;
    size_t childSize = child->size();
    while(parent) {
      parent->mSize += childSize;
      parent = parent->mOps.mParent;
    }
    child->mOps.mParent = this;
    mChildren.emplace_back(std::move(child));
  }

  size_t Node::size() const {
    return mSize + _size();
  }

  void Node::copyConstructToBuffer(const void* base, void* buffer) const {
    _funcToBuffer(&Node::_copyConstruct, base, buffer);
  }

  void Node::copyConstructFromBuffer(void* base, const void* buffer, NodeDiff diff) const {
    _funcFromBuffer(&Node::_copyConstruct, base, buffer, diff);
  }

  void Node::_funcToBuffer(void (Node::* func)(const void*, void*) const, const void* base, void* buffer) const {
    (this->*func)(base, buffer);
    _translateBase(base);
    buffer = Util::offset(buffer, _size());
    for(const auto& child : mChildren) {
      child->_funcToBuffer(func, Util::offset(base, child->mOps.mOffset), buffer);
      buffer = Util::offset(buffer, child->size());
    }
  }

  void Node::_funcFromBuffer(void (Node::* func)(const void*, void*) const, void* base, const void* buffer, NodeDiff diff) const {
    int nodeIndex = 0;
    _funcFromBuffer(func, base, buffer, diff, nodeIndex);
  }

  void Node::_funcFromBuffer(void (Node::* func)(const void*, void*) const, void* base, const void* buffer, NodeDiff diff, int& nodeIndex) const {
    base = Util::offset(base, mOps.mOffset);
    if(mChildren.empty()) {
      if(diff & (static_cast<NodeDiff>(1) << nodeIndex))
        (this->*func)(buffer, base);
      ++nodeIndex;
    }
    else {
      _translateBase(base);
      buffer = Util::offset(buffer, _size());
      for(const auto& child : mChildren) {
        child->_funcFromBuffer(func, base, buffer, diff, nodeIndex);
        buffer = Util::offset(buffer, child->size());
      }
    }
  }

  void Node::copyToBuffer(const void* base, void* buffer) const {
    _funcToBuffer(&Node::_copy, base, buffer);
  }

  void Node::copyFromBuffer(void* base, const void* buffer, NodeDiff diff) const {
    _funcFromBuffer(&Node::_copy, base, buffer, diff);
  }

  void Node::copyConstructBufferToBuffer(const void* from, void* to) const {
    _funcBufferToBuffer(&Node::_copyConstruct, from, to);
  }

  void Node::copyBufferToBuffer(const void* from, void* to) const {
    _funcBufferToBuffer(&Node::_copy, from, to);
  }

  void Node::_funcBufferToBuffer(void (Node::* func)(const void*, void*) const, const void* from, void* to) const {
    if(mChildren.empty()) {
      (this->*func)(from, to);
    }
    else {
      for(const auto& child : mChildren) {
        child->_funcBufferToBuffer(func, from, to);
        size_t childSize = child->size();
        from = Util::offset(from, childSize);
        to = Util::offset(to, childSize);
      }
    }
  }

  void Node::destructBuffer(void* buffer) const {
    //Only leaf nodes have values to destruct
    if(mChildren.empty()) {
      _destruct(buffer);
    }
    else {
      for(const auto& child : mChildren) {
        child->destructBuffer(buffer);
        buffer = Util::offset(buffer, child->size());
      }
    }
  }

  NodeDiff Node::getDiff(const void* base, const void* other) const {
    int nodeIndex = 0;
    return _getDiff(base, other, nodeIndex);
  }

  void Node::forEachDiff(NodeDiff diff, const void* base, const DiffCallback& callback) const {
    int nodeIndex = 0;
    _forEachDiff(diff, base, callback, nodeIndex);
  }

  void Node::copyFromDiff(NodeDiff diff, const void* from, void* to) const {
    //Walk through each diff node in from, then translate the base of to to that node and copy the data
    forEachDiff(diff, from, [to](const Lua::Node& node, const void* base) {
      node._copy(base, node._translateBaseToNode(to));
    });
  }

  NodeDiff Node::_getDiff(const void* base, const void* other, int& nodeIndex) const {
    assert(nodeIndex < 64 && "NodeDiff is only big enough to hold 64 node diffs");
    NodeDiff result = 0;
    //Leaf nodes have values, write diff to bitfield
    if(mChildren.empty()) {
      result = !_equals(base, other) ? static_cast<NodeDiff>(1) << nodeIndex : 0;
      ++nodeIndex;
    }
    else {
      _translateBase(base);
      _translateBase(other);
      for(const auto& child : mChildren) {
         result |= child->_getDiff(Util::offset(base, child->mOps.mOffset), Util::offset(other, child->mOps.mOffset), nodeIndex);
      }
    }

    return result;
  }

  void Node::_forEachDiff(NodeDiff diff, const void* base, const DiffCallback& callback, int& nodeIndex) const {
    //Leaf nodes have values, write diff to bitfield
    if(mChildren.empty()) {
      if(diff & (static_cast<NodeDiff>(1) << nodeIndex))
        callback(*this, base);
      ++nodeIndex;
    }
    else {
      _translateBase(base);
      for(const auto& child : mChildren) {
         child->_forEachDiff(diff, Util::offset(base, child->mOps.mOffset), callback, nodeIndex);
      }
    }
  }

  const void* Node::_translateBaseToNode(const void* base) const {
    //From the child build a top down stack of parents
    std::stack<const Node*, std::vector<const Node*, LIFOAllocator<const Node*>>> parents;
    const Node* node = this;
    while(node) {
      parents.push(node);
      node = node->mOps.mParent;
    }

    //Walk down the heirarchy from the root offsetting the pointer
    while(!parents.empty()) {
      const Node* node = parents.top();
      node->_translateBase(base);
      base = Util::offset(base, node->mOps.mOffset);
      parents.pop();
    }
    return base;
  }

  void* Node::_translateBaseToNode(void* base) const {
    return const_cast<void*>(_translateBaseToNode(const_cast<const void*>(base)));
  }

  const void* Node::_translateBufferToNode(const void* buffer) const {
    size_t bytes = 0;
    _forEachDepthFirstToChild(&Node::_countNodeSizes, &bytes);
    return Util::offset(buffer, bytes);
  }

  void* Node::_translateBufferToNode(void* buffer) const {
    return const_cast<void*>(_translateBufferToNode(const_cast<const void*>(buffer)));
  }

  NodeDiff Node::_getDiffId() const {
    size_t count = 0;
    _forEachDepthFirstToChild(&Node::_countNodes, &count);
    return static_cast<NodeDiff>(1) << count;
  }

  void Node::_forEachDepthFirstToChild(void (Node::* func)(const Node&, void*) const, void* data) const {
    //From the child build a top down stack of parents
    std::stack<const Node*, std::vector<const Node*, LIFOAllocator<const Node*>>> parents;
    const Node* node = this;
    while(node) {
      parents.push(node);
      node = node->mOps.mParent;
    }

    size_t offset = 0;
    while(parents.size() > 1) {
      const Node* node = parents.top();
      parents.pop();
      const Node* traverseChild = parents.top();
      parents.pop();
      for(const auto& child : node->mChildren) {
        if(child.get() == traverseChild)
          break;
        else
          (this->*func)(*child, data);
      }
    }
  }

  void Node::_countNodes(const Node& node, void* data) const {
    ++(*reinterpret_cast<size_t*>(data));
  }

  void Node::_countNodeSizes(const Node& node, void* data) const {
    *reinterpret_cast<size_t*>(data) += node._size();
  }

  void Node::getField(lua_State* s, const std::string& field, SourceType source) const {
    //If source is from stack then there's nothing to do
    if(source == SourceType::FromStack)
      return;
    if(!mOps.mParent) {
      if(source == SourceType::FromGlobal)
        lua_getglobal(s, field.c_str());
      //else value is expected to already be on top of the stack
    }
    else
      lua_getfield(s, -1, field.c_str());
  }

  void Node::setField(lua_State* s, const std::string& field, SourceType source) const {
    //If desired destination is stack then there's nothing to do
    if(source == SourceType::FromStack)
      return;
    if(!mOps.mParent) {
      if(source == SourceType::FromGlobal)
        lua_setglobal(s, field.c_str());
      //else value is left on top of stack
    }
    else
      lua_setfield(s, -2, field.c_str());
  }

  void Node::getField(lua_State* s, SourceType source) const {
    getField(s, mOps.mName, source);
  }

  void Node::setField(lua_State* s, SourceType source) const {
    setField(s, mOps.mName, source);
  }

  void Node::readFromLua(lua_State* s, void* base, SourceType source) const {
    getField(s, source);
    _readFromLua(s, base);
    _translateBase(base);
    for(auto& child : mChildren)
      child->readFromLua(s, Util::offset(base, child->mOps.mOffset));
    lua_pop(s, 1);
  }

  void Node::writeToLua(lua_State* s, const void* base, SourceType source) const {
    _writeToLua(s, base);
    _translateBase(base);
    if(mChildren.size()) {
      //Make new table and allow children to fill it
      lua_newtable(s);
      for(auto& child : mChildren) {
        child->writeToLua(s, Util::offset(base, child->mOps.mOffset));
        child->setField(s);
      }
      //Parent will store the result of this write in their table. If there is no parent we must do it here
      if(!mOps.mParent)
        setField(s, source);
    }
  }

  void Node::readFromLuaToBuffer(lua_State* s, void* buffer, SourceType source) const {
    getField(s, source);
    //Leaf nodes have values
    if(mChildren.empty()) {
      //Construct in buffer so there's a valid object there for assignment
      _defaultConstruct(buffer);
      _readFromLua(s, buffer);
    }
    else {
      for(const auto& child : mChildren) {
        child->readFromLuaToBuffer(s, buffer);
        buffer = Util::offset(buffer, child->size());
      }
    }
  }

  const Node* Node::getChild(const char* child) const {
    for(const auto& c : mChildren)
      if(!std::strcmp(child, c->getName().c_str()))
        return c.get();
    return nullptr;
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
    _cast(base) = reinterpret_cast<size_t>(lua_touserdata(s, -1));
  }

  void LightUserdataSizetNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushlightuserdata(s, reinterpret_cast<void*>(_cast(base)));
  }

  void BoolNode::_readFromLua(lua_State* s, void* base) const {
    *static_cast<bool*>(base) = static_cast<bool>(lua_toboolean(s, -1));
  }

  void BoolNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushboolean(s, static_cast<int>(*static_cast<const bool*>(base)));
  }

  void SizetNode::_readFromLua(lua_State* s, void* base) const {
    _cast(base) = static_cast<size_t>(lua_tointeger(s, -1));
  }

  void SizetNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushinteger(s, static_cast<lua_Integer>(_cast(base)));
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
  }
}