#include "Precompile.h"
#include "lua/lib/LuaNumVec.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaUtil.h"

#include <lua.hpp>

struct lua_State;

namespace Lua {
  const char* NumVec::CLASS_NAME = "NumVec";

  NumVec::NumVec(size_t reserve) {
    mVec.reserve(reserve);
  }

  NumVec::~NumVec() {
  }

  std::string NumVec::toString() const {
    std::string result;
    //Guess 8 chars per number
    result.reserve(static_cast<size_t>(mVec.size()*8));
    result = CLASS_NAME;
    result += " [ ";
    for(float v : mVec) {
      result += std::to_string(v) + ", ";
    }
    if(mVec.size())
      result += std::to_string(mVec.back());
    result += " ]";
    return result;
  }

  void NumVec::openLib(lua_State* l) {
    StackAssert sa(l);
    luaL_Reg statics[] = {
      { "new", NumVec::construct },
      { nullptr, nullptr }
    };
    luaL_Reg members[] = {
      { "get", NumVec::get },
      { "set", NumVec::set },
      { "size", NumVec::size },
      { "empty", NumVec::empty },
      { "clear", NumVec::clear },
      { "find", NumVec::find },
      { "resize", NumVec::resize },
      { "reserve", NumVec::reserve },
      { "pushBack", NumVec::pushBack },
      { "popBack", NumVec::popBack },
      { "__tostring", NumVec::toString },
      { "__index", NumVec::indexOverload },
      { "__newindex", NumVec::newindexOverload },
      { "__len", NumVec::size },
      { "__gc", NumVec::destruct },
      { nullptr, nullptr }
    };
    Util::registerClass(l, statics, members, CLASS_NAME);
  }

  int NumVec::construct(lua_State* l) {
    int reserveArg = 0;
    if(lua_isinteger(l, 1))
      reserveArg = std::max(0, static_cast<int>(lua_tointeger(l, 1)));

    void* data = lua_newuserdata(l, sizeof(NumVec));
    NumVec* v = new (data) NumVec(reserveArg);
    luaL_setmetatable(l, CLASS_NAME);
    return 1;
  }

  int NumVec::destruct(lua_State* l) {
    _getVec(l, 1)->~NumVec();
    return 0;
  }

  //Number get(array, index)
  int NumVec::get(lua_State* l) {
    NumVec* v = _getVec(l, 1);
    lua_pushnumber(l, v->mVec[_getIndex(l, *v, 2) - 1]);
    return 1;
  }

  //void set(array, index, value)
  int NumVec::set(lua_State* l) {
    NumVec* v = _getVec(l, 1);
    v->mVec[_getIndex(l, *v, 2) - 1] = _getValue(l, 3);
    return 0;
  }

  //Number size()
  int NumVec::size(lua_State* l) {
    lua_pushnumber(l, _getVec(l, 1)->mVec.size());
    return 1;
  }

  //string __tostring(array)
  int NumVec::toString(lua_State* l) {
    NumVec* v = _getVec(l, 1);
    std::string s = v->toString();
    lua_pushlstring(l, s.c_str(), s.size());
    return 1;
  }

  //number __index(array, index)
  //function __index(array, string)
  int NumVec::indexOverload(lua_State* l) {
    return Util::intIndexOverload(l, get);
  }

  //void __newindex(array, index, number)
  //void __newindex(array, key, value)
  int NumVec::newindexOverload(lua_State* l) {
    return Util::intNewIndexOverload(l, set);
  }

  //bool empty()
  int NumVec::empty(lua_State* l) {
    lua_pushboolean(l, _getVec(l, 1)->mVec.empty());
    return 1;
  }

  //void clear()
  int NumVec::clear(lua_State* l) {
    _getVec(l, 1)->mVec.clear();
    return 0;
  }

  //int find(number)
  int NumVec::find(lua_State* l) {
    NumVec* v = _getVec(l, 1);
    float val = _getValue(l, 2);
    auto it = std::find(v->mVec.begin(), v->mVec.end(), val);
    lua_pushnumber(l, it != v->mVec.end() ? it - v->mVec.begin() + 1 : 0);
    return 1;
  }

  //void resize(int)
  int NumVec::resize(lua_State* l) {
    _getVec(l, 1)->mVec.resize(_getSize(l, 2));
    return 0;
  }

  //void reserve(int)
  int NumVec::reserve(lua_State* l) {
    _getVec(l, 1)->mVec.reserve(_getSize(l, 2));
    return 0;
  }

  int NumVec::pushBack(lua_State* l) {
    _getVec(l, 1)->mVec.push_back(_getValue(l, 2));
    return 0;
  }

  int NumVec::popBack(lua_State* l) {
    _getVec(l, 1)->mVec.pop_back();
    return 0;
  }

  NumVec* NumVec::_getVec(lua_State* l, int i) {
    return static_cast<NumVec*>(luaL_checkudata(l, i, CLASS_NAME));
  }

  int NumVec::_getIndex(lua_State* l, const NumVec& v, int i) {
    int index = static_cast<int>(luaL_checkinteger(l, i));
    luaL_argcheck(l, index > 0 && index <= static_cast<int>(v.mVec.size()), i, "Out of bounds");
    return index;
  }

  float NumVec::_getValue(lua_State* l, int i) {
    return static_cast<float>(luaL_checknumber(l, i));
  }

  size_t NumVec::_getSize(lua_State* l, int i) {
    size_t size = static_cast<size_t>(luaL_checknumber(l, i));
    luaL_argcheck(l, i >= 0, i, "Size must be positive");
    return size;
  }
}