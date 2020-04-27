#include "Precompile.h"
#include "lua/LuaNode.h"

#include "allocator/LIFOAllocator.h"
#include "lua/LuaState.h"
#include "lua/LuaStackAssert.h"
#include "lua/lib/LuaVec3.h"
#include "lua/lib/LuaQuat.h"

#include <lua.hpp>
#include <SyxVec3.h>
#include <SyxQuat.h>

namespace Lua {
  NodeOps::NodeOps(Node* parent, std::string&& name, int index, size_t offset)
    : mParent(parent)
    , mName(std::move(name))
    , mIndex(index)
    , mOffset(offset) {
  }

  NodeOps::NodeOps(Node& parent, std::string&& name, size_t offset)
    : NodeOps(&parent, std::move(name), -1, offset) {
  }

  NodeOps::NodeOps(Node& parent, int index, size_t offset)
    : NodeOps(&parent, {}, index, offset) {
  }

  NodeOps::NodeOps(std::string&& name)
    : NodeOps(nullptr, std::move(name), -1, 0) {
  }

  NodeOps::NodeOps(const std::string& name)
    : NodeOps(nullptr, std::move(std::string(name)), -1, 0) {
  }

  void NodeOps::pushKey(lua_State* s) const {
    if(mName.empty())
      lua_pushinteger(s, mIndex);
    else
      lua_pushstring(s, mName.c_str());
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

  void Node::forEachChildShallow(std::function<void(const Node&)> callback) const {
    for(const auto& child : mChildren) {
      callback(*child);
    }
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
    if(mChildren.empty()) {
      (this->*func)(base, buffer);
    }
    else {
      _translateBase(base);
      if(!base)
        return;
      buffer = Util::offset(buffer, _size());
      for(const auto& child : mChildren) {
        child->_funcToBuffer(func, Util::offset(base, child->mOps.mOffset), buffer);
        buffer = Util::offset(buffer, child->size());
      }
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
      if(!base)
        return;
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
      auto offsetBuffers = [&from, &to](size_t bytes) {
        from = Util::offset(from, bytes);
        to = Util::offset(to, bytes);
      };
      offsetBuffers(_size());
      for(const auto& child : mChildren) {
        child->_funcBufferToBuffer(func, from, to);
        offsetBuffers(child->size());
      }
    }
  }

  void Node::_forEachBottomUp(void (Node::* func)(void*) const, void* base) const {
    std::queue<std::pair<const Node*, void*>> toTraverse;
    std::deque<std::pair<const Node*, void*>> nodes;
    toTraverse.push({ this, base });

    //Traverse tree and put each node breadth first into 'nodes'
    while(!toTraverse.empty()) {
      nodes.push_back(toTraverse.front());

      const Node* node = toTraverse.front().first;
      void* data = toTraverse.front().second;
      toTraverse.pop();

      node->_translateBase(data);
      if(data) {
        for(const auto& child : node->mChildren) {
          toTraverse.push({ child.get(), Util::offset(data, child->mOps.mOffset) });
        }
      }
    }

    //nodes are depth first traversal, walk it backwards for a bottom up traversal
    while(!nodes.empty()) {
      (nodes.back().first->*func)(nodes.back().second);
      nodes.pop_back();
    }
  }

  void Node::destructBuffer(void* buffer) const {
    if(mChildren.empty()) {
      _destruct(buffer);
    }
    else {
      buffer = Util::offset(buffer, _size());
      for(const auto& child : mChildren) {
        child->destructBuffer(buffer);
        buffer = Util::offset(buffer, child->size());
      }
    }
  }

  void Node::defaultConstruct(void* base) const {
    int nodeIndex = 0;
    _defaultConstruct(base);
    _forEachDiff(~Lua::NodeDiff(0), base, [](const Node& node, const void* data) {
      node._defaultConstruct(const_cast<void*>(data));
    }, nodeIndex);
  }

  void Node::destruct(void* base) const {
    //If this owns child memory that means destruction should go all the way down, so no further recursion needed
    if(_ownsChildMemory()) {
      _destruct(base);
    }
    else {
      _translateBase(base);
      if(base) {
        for(const auto& child : mChildren) {
          child->destruct(Util::offset(base, child->mOps.mOffset));
        }
      }
    }
  }

  size_t Node::getTypeId() const {
    return typeId<void>();
  }

  NodeDiff Node::getDiff(const void* base, const void* other) const {
    int nodeIndex = 0;
    return _getDiff(base, other, nodeIndex);
  }

  void Node::forEachDiff(NodeDiff diff, const void* base, const DiffCallback& callback) const {
    int nodeIndex = 0;
    _forEachDiff(diff, base, callback, nodeIndex);
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
      if(base && other) {
        for(const auto& child : mChildren) {
           result |= child->_getDiff(Util::offset(base, child->mOps.mOffset), Util::offset(other, child->mOps.mOffset), nodeIndex);
        }
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
      if(base) {
        for(const auto& child : mChildren) {
           child->_forEachDiff(diff, Util::offset(base, child->mOps.mOffset), callback, nodeIndex);
        }
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
      node = parents.top();
      base = Util::offset(base, node->mOps.mOffset);
      parents.pop();
      //If this is the destination, don't translate, as translate sets up traversal for children
      if(!parents.empty())
        node->_translateBase(base);
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

  const void* Node::offset(const void* base) const {
    return Util::offset(base, mOps.mOffset);
  }

  void* Node::offset(void* base) const {
    return Util::offset(base, mOps.mOffset);
  }

  void Node::_forEachDepthFirstToChild(void (Node::* func)(const Node&, void*) const, void* data) const {
    //From the child build a top down stack of parents
    std::stack<const Node*, std::vector<const Node*, LIFOAllocator<const Node*>>> parents;
    const Node* node = this;
    while(node) {
      parents.push(node);
      node = node->mOps.mParent;
    }

    while(parents.size() > 1) {
      node = parents.top();
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

  void Node::_countNodes(const Node&, void* data) const {
    ++(*reinterpret_cast<size_t*>(data));
  }

  void Node::_countNodeSizes(const Node& node, void* data) const {
    *reinterpret_cast<size_t*>(data) += node._size();
  }

  void Node::getField(lua_State* s, SourceType source) const {
    //If source is from stack then there's nothing to do
    if(source == SourceType::FromStack) {
      //Need to push something so stack size stays the same
      lua_pushvalue(s, -1);
    }
    else if(source == SourceType::FromGlobal) {
      assert(!mOps.mName.empty() && "Globals must have string keys");
      lua_getglobal(s, mOps.mName.c_str());
    }
    else if(!mOps.mParent) {
      //value is expected to already be on top of the stack
      lua_pushvalue(s, -1);
    }
    else {
      mOps.pushKey(s);
      lua_gettable(s, -2);
    }
  }

  void Node::setField(lua_State* s, SourceType source) const {
    //If desired destination is stack then there's nothing to do
    if(source == SourceType::FromStack)
      return;
    if(!mOps.mParent) {
      if(source == SourceType::FromGlobal) {
        assert(!mOps.mName.empty() && "Globals must have string keys");
        lua_setglobal(s, mOps.mName.c_str());
      }
      //else value is left on top of stack
    }
    else {
      mOps.pushKey(s);
      //push value, which is under key
      lua_pushvalue(s, -2);
      lua_settable(s, -4);
      //pop extra value that was sitting under key
      lua_pop(s, 1);
    }
  }

  bool Node::readFromLua(lua_State* s, void* base, SourceType source) const {
    Lua::StackAssert sa(s);
    getField(s, source);
    bool gotField = lua_type(s, -1) != LUA_TNIL;
    if(gotField) {
      _readFromLua(s, base);
      _translateBase(base);
      for(auto& child : mChildren)
        gotField = child->readFromLua(s, Util::offset(base, child->mOps.mOffset)) && gotField;
    }
    lua_pop(s, 1);
    return gotField;
  }

  void Node::writeToLua(lua_State* s, const void* base, SourceType source) const {
    Lua::StackAssert sa(s, source == SourceType::FromGlobal ? 0 : 1);
    _writeToLua(s, base);
    _translateBase(base);
    if(mChildren.size()) {
      assert(lua_type(s, -1) == LUA_TTABLE && "Nodes with children should write tables in _writeToLua");
      for(auto& child : mChildren) {
        child->writeToLua(s, Util::offset(base, child->mOps.mOffset));
        child->setField(s);
      }
      //Parent will store the result of this write in their table. If there is no parent we must do it here
      if(!mOps.mParent)
        setField(s, source);
    }
  }

  bool Node::readFromLuaToBuffer(lua_State* s, void* buffer, SourceType source) const {
    Lua::StackAssert sa(s);
    bool gotField = lua_type(s, -1) != LUA_TNIL;
    if(gotField)
      getField(s, source);
    gotField = lua_type(s, -1) != LUA_TNIL;

    //Construct in buffer so there's a valid object there for assignment
    //Need to make sure that entire struct is default constructed even if lua values are missing
    _defaultConstruct(buffer);
    if(gotField)
      _readFromLua(s, buffer);

    for(const auto& child : mChildren) {
        gotField = child->readFromLuaToBuffer(s, buffer) && gotField;
        buffer = Util::offset(buffer, child->size());
    }

    lua_pop(s, 1);
    return gotField;
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

  bool Node::hasInspector() const {
    return mInspector != nullptr;
  }

  bool Node::inspect(void* prop) const {
    return mInspector(mOps.mName.c_str(), prop);
  }

  Node& Node::setInspector(std::function<bool(const char*, void*)> inspector) {
    mInspector = std::move(inspector);
    return *this;
  }

  void RootNode::_writeToLua(lua_State* s, const void*) const {
    // Write table for children to fill
    lua_newtable(s);
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
    //Either are acceptable, since some userdata can only be serialized as a number
    switch(lua_type(s, -1)) {
      case LUA_TLIGHTUSERDATA: _cast(base) = reinterpret_cast<size_t>(lua_touserdata(s, -1)); break;
      case LUA_TNUMBER: _cast(base) = static_cast<size_t>(lua_tointeger(s, -1)); break;
      default: break;
    }
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

  void DoubleNode::_readFromLua(lua_State* s, void* base) const {
    _cast(base) = static_cast<lua_Number>(lua_tonumber(s, -1));
  }

  void DoubleNode::_writeToLua(lua_State* s, const void* base) const {
    lua_pushnumber(s, static_cast<lua_Number>(_cast(base)));
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