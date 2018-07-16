#include "Precompile.h"
#include "lua/LuaVariant.h"

#include <lua.hpp>
#include "lua/LuaNode.h"
#include "lua/LuaStackAssert.h"

namespace Lua {
  Variant::Variant()
    : mType(nullptr) {
  }

  Variant::Variant(const Key& key)
    : mKey(key)
    , mType(nullptr) {
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
    mKey = rhs.mKey;
    mType = rhs.mType;
    mChildren = rhs.mChildren;
    _copyData(rhs.mData);
    return *this;
  }

  Variant& Variant::operator=(Variant&& rhs) {
    mKey = std::move(rhs.mKey);
    mType = rhs.mType;
    mChildren = std::move(rhs.mChildren);
    _moveData(rhs.mData);
    return *this;
  }

  bool Variant::readFromLua(lua_State* l) {
    StackAssert sa(l);
    clear();
    switch(lua_type(l, -1)) {
      case LUA_TSTRING: mType = &StringNode::singleton(); break;
      case LUA_TBOOLEAN: mType = &BoolNode::singleton(); break;
      case LUA_TLIGHTUSERDATA: mType = &LightUserdataSizetNode::singleton(); break;
      case LUA_TNUMBER: mType = &DoubleNode::singleton(); break;
      case LUA_TUSERDATA: /* TODO: call a function on the member to get the appropriate node */ break;
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
    assert(mData.data() == orig && "Move should always transfer memory, if not, this is unsafe and a copy must be used");
  }
}