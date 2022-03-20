#include "Precompile.h"
#include "lua/LuaVariant.h"

#include <lua.hpp>
#include "lua/LuaStackAssert.h"

namespace Lua {
  Variant::Variant()
    : mType(nullptr) {
  }

  Variant::Variant(Key key, const Node* type)
    : mKey(std::move(key))
    , mType(type) {
    if(mType) {
      mData.resize(mType->_size());
      mType->_defaultConstruct(mData.data());
    }
  }

  Variant::Variant(const Variant& rhs)
    : mKey(rhs.mKey)
    , mType(rhs.mType)
    , mChildren(rhs.mChildren) {
    _copyData(rhs.mData);
  }

  Variant::Variant(Variant&& rhs)
    : mKey(std::move(rhs.mKey))
    , mType(rhs.mType)
    , mChildren(std::move(rhs.mChildren)) {
    _moveData(rhs.mData);
  }

  Variant::~Variant() {
    _destructData();
  }

  Variant& Variant::operator=(const Variant& rhs) {
    //TODO: this doesn't handle its previous value properly
    mKey = rhs.mKey;
    mType = rhs.mType;
    mChildren = rhs.mChildren;
    _copyData(rhs.mData);
    return *this;
  }

  Variant& Variant::operator=(Variant&& rhs) {
    //TODO: this doesn't handle its previous value properly
    mKey = std::move(rhs.mKey);
    mType = rhs.mType;
    mChildren = std::move(rhs.mChildren);
    _moveData(rhs.mData);
    return *this;
  }

  bool Variant::operator==(const Variant& rhs) const {
    if(mKey != rhs.mKey || mType != rhs.mType || mChildren.size() != rhs.mChildren.size() || mData.size() != rhs.mData.size())
      return false;
    if(mType && !mType->_equals(mData.data(), rhs.mData.data()))
      return false;
    return mChildren == rhs.mChildren;
  }

  bool Variant::operator!=(const Variant& rhs) const {
    return !(*this == rhs);
  }

  bool Variant::readFromLua(lua_State* l) {
    StackAssert sa(l);
    clear();
    switch(lua_type(l, -1)) {
      case LUA_TSTRING: mType = &StringNode::singleton(); break;
      case LUA_TBOOLEAN: mType = &BoolNode::singleton(); break;
      case LUA_TLIGHTUSERDATA: mType = &LightUserdataSizetNode::singleton(); break;
      case LUA_TNUMBER: mType = &DoubleNode::singleton(); break;
      case LUA_TUSERDATA: {
        Lua::StackAssert mem(l);
        if(lua_getmetatable(l, -1)) {
          if(lua_getfield(l, -1, "__typeNode") == LUA_TFUNCTION) {
            //push userdata
            lua_pushvalue(l, -3);
            if(lua_pcall(l, 1, 1, 0) == LUA_OK && lua_type(l, -1) == LUA_TLIGHTUSERDATA) {
              mType = reinterpret_cast<Lua::Node*>(lua_touserdata(l, -1));
            }
            //Pop function result or error
            lua_pop(l, 1);
          }
          //If function wasn't there, pop the nil
          else {
            lua_pop(l, 1);
          }
          //Pop metatable
          lua_pop(l, 1);
        }
        break;
      }
      case LUA_TTABLE: {
        lua_pushnil(l);
        while(lua_next(l, -2)) {
          Key key;
          if(key.readFromLua(l, -2)) {
            Variant child(key);
            //If we could read a value from the table, add it to children, otherwise throw it out
            if(child.readFromLua(l)) {
              mChildren.emplace_back(std::move(child));
            }
          }
          lua_pop(l, 1);
          const char* type = lua_typename(l, lua_type(l, -1));
          type;
        }
        return !mChildren.empty();
      }
    }

    if(mType) {
      mData.resize(mType->_size());
      mType->readFromLuaToBuffer(l, mData.data());
    }
    return mType != nullptr;
  }

  void Variant::writeToLua(lua_State* l) const {
    StackAssert sa(l, 1);
    if(mType) {
      mType->writeToLua(l, mData.data());
    }
    else {
      lua_newtable(l);
      for(const Variant& child : mChildren) {
        child.mKey.push(l);
        child.writeToLua(l);
        lua_settable(l, -3);
      }
    }
  }

  void Variant::clear() {
    mChildren.clear();
    _destructData();
    mType = nullptr;
  }

  size_t Variant::getTypeId() const {
    return mType ? mType->getTypeId() : typeId<void>();
  }

  const Key& Variant::getKey() const {
    return mKey;
  }

  const Variant* Variant::getChild(const Key& key) const {
    if(const auto child = std::find_if(mChildren.begin(), mChildren.end(), [&key](auto&& c) { return key == c.mKey; }); child != mChildren.end()) {
      return &*child;
    }
    return nullptr;
  }

  Variant* Variant::getChild(const Key& key) {
    return const_cast<Variant*>(const_cast<const Variant*>(this)->getChild(key));
  }

  const Lua::Node* Variant::getType() const {
    return mType;
  }

  void Variant::forEachChild(std::function<void(const Variant&)> callback) const {
    for(const Variant& child : mChildren)
      callback(child);
  }

  void Variant::forEachChild(std::function<void(Variant&)> callback) {
    for(Variant& child : mChildren)
      callback(child);
  }

  void Variant::addChild(Variant child) {
    mChildren.emplace_back(std::move(child));
  }

  const void* Variant::data() const {
    return mData.data();
  }

  void* Variant::data() {
    return mData.data();
  }

  void Variant::_destructData() {
    if(mType && mData.size()) {
      mType->destruct(mData.data());
    }
    mData.clear();
  }

  void Variant::_copyData(const std::vector<uint8_t>& from) {
    if(mType) {
        mData.resize(mType->_size());
        mType->copyConstructBufferToBuffer(from.data(), mData.data());
    }
  }

  void Variant::_moveData(std::vector<uint8_t>& from) {
    const uint8_t* orig = from.data();
    mData = std::move(from);
    orig;
    assert(mData.data() == orig && "Move should always transfer memory, if not, this is unsafe and a copy must be used");
  }
}