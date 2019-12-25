#include "Precompile.h"
#include "lua/lib/LuaNumArray.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaUtil.h"

#include <lua.hpp>

namespace Lua {
  const char* NumArray::CLASS_NAME = "NumArray";

  NumArray::NumArray(size_t size)
    : mSize(size) {
    std::memset(mData, 0, sizeof(float)*mSize);
  }

  int NumArray::size() const {
    return mSize;
  }

  float NumArray::get(int i) const {
    return mData[i - 1];
  }

  void NumArray::set(int i, float value) {
    mData[i - 1] = value;
  }

  std::string NumArray::toString() const {
    std::string result;
    //Guess 8 chars per number
    result.reserve(static_cast<size_t>(mSize*8));
    result = CLASS_NAME;
    result += " [ ";
    for(int i = 0; i + 1 < mSize; ++i) {
      result += std::to_string(mData[i]) + ", ";
    }
    if(mSize)
      result += std::to_string(mData[mSize - 1]);
    result += " ]";
    return result;
  }

  void NumArray::openLib(lua_State* l) {
    Lua::StackAssert sa(l);
    const luaL_Reg statics[] = {
      { "new", NumArray::construct },
      { "size", NumArray::size },
      { "get", NumArray::get },
      { "set", NumArray::set },
      { nullptr, nullptr }
    };
    const luaL_Reg members[] = {
      { "size", NumArray::size },
      { "get", NumArray::get },
      { "set", NumArray::set },
      { "__tostring", NumArray::toString },
      { "__index", NumArray::indexOverload },
      { "__newindex", NumArray::newindexOverload },
      { "__len", NumArray::size },
      { nullptr, nullptr }
    };
    Util::registerClass(l, statics, members, CLASS_NAME);
    luaL_newmetatable(l, CLASS_NAME);
    //Register member functions
    luaL_setfuncs(l, members, 0);
    lua_pop(l, 1);

    //Register all static functions
    luaL_newlib(l, statics);
    lua_setglobal(l, CLASS_NAME);
  }

  int NumArray::construct(lua_State* l) {
    int size = std::max(0, static_cast<int>(luaL_checkinteger(l, 1)));
    void* data = static_cast<NumArray*>(lua_newuserdata(l, sizeof(NumArray) + (0, size - 1)*sizeof(float)));
    luaL_setmetatable(l, CLASS_NAME);
    new (data) NumArray(size);
    return 1;
  }

  //Number get(array, index)
  int NumArray::get(lua_State* l) {
    NumArray* arr = _getArray(l, 1);
    lua_pushnumber(l, static_cast<lua_Number>(arr->get(_getIndex(l, *arr, 2))));
    return 1;
  }

  //void set(array, index, value)
  int NumArray::set(lua_State* l) {
    NumArray* arr = _getArray(l, 1);
    int index = _getIndex(l, *arr, 2);
    float value = _getValue(l, 3);
    arr->set(index, value);
    return 0;
  }

  //Number size()
  int NumArray::size(lua_State* l) {
    NumArray* arr = _getArray(l, 1);
    lua_pushnumber(l, arr->size());
    return 1;
  }

  //string __tostring(array)
  int NumArray::toString(lua_State* l) {
    NumArray* arr = _getArray(l, 1);
    std::string str = arr->toString();
    lua_pushlstring(l, str.c_str(), str.size());
    return 1;
  }

  //number __index(array, index)
  //function __index(array, string)
  int NumArray::indexOverload(lua_State* l) {
    return Util::intIndexOverload(l, get);
  }

  //void __newindex(array, index, number)
  //void __newindex(array, key, value)
  int NumArray::newindexOverload(lua_State* l) {
    return Util::intNewIndexOverload(l, set);
  }

  NumArray* NumArray::_getArray(lua_State* l, int i) {
    return static_cast<NumArray*>(luaL_checkudata(l, i, CLASS_NAME));
  }

  int NumArray::_getIndex(lua_State* l, const NumArray& arr, int i) {
    int result = static_cast<int>(luaL_checkinteger(l, i));
    luaL_argcheck(l, result > 0 && result <= arr.size(), i, "Index out of bounds");
    return result;
  }

  float NumArray::_getValue(lua_State* l, int i) {
    return static_cast<float>(luaL_checknumber(l, i));
  }
}