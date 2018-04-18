#include "Precompile.h"
#include "lua/LuaSerializer.h"

#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include <lua.hpp>

namespace Lua {
  Serializer::Serializer(const std::string tab, const std::string& newline, int numPrecision)
    : mTab(tab)
    , mNewline(newline)
    , mS(nullptr)
    , mBuffer(nullptr)
    , mDepth(0) {
    mRoundDigits = 1;
    for(int i = 0; i < numPrecision; ++i)
      mRoundDigits *= 10;
    mEpsilon = 1.0/static_cast<double>(mRoundDigits);
  }

  void Serializer::serializeGlobal(lua_State* s, const std::string& global, std::string& buffer) {
    Lua::StackAssert sa(s);
    lua_getglobal(s, global.c_str());
    buffer += global + " = ";
    serializeTop(s, buffer);
  }

  void Serializer::serializeTop(lua_State* s, std::string& buffer) {
    Lua::StackAssert sa(s);
    mS = s;
    mBuffer = &buffer;
    mDepth = 0;
    //Copy the value as _serialize will pop it
    lua_pushvalue(s, -1);
    _serialize();
    mBuffer = nullptr;
    mS = nullptr;
  }

  void Serializer::_serialize() {
    lua_State* s = mS;
    int type = lua_type(s, -1);
    if(type != LUA_TTABLE)
      return _serializeValue();

    ++mDepth;

    *mBuffer += "{";
    int tableIndex = lua_gettop(s);
    std::vector<std::string> keys;
    //Dummy value for next to pop off
    lua_pushnil(s);
    bool first = true;
    while(lua_next(s, tableIndex)) {
      //Key is now at -2 and value at -1
      int keyType = lua_type(s, -2);

      //If it's a number this is an array, serialize now
      if(keyType == LUA_TNUMBER) {
        _csv(first);
        _serialize();
      }
      else if(keyType == LUA_TSTRING) {
      //Gather string values so we can write them alphabetically
        int pre = lua_gettop(s);
        keys.push_back(lua_tostring(s, -2));
        int post = lua_gettop(s);
        assert(pre == post);
        lua_pop(s, 1);
      }
      else {
        //Not sure what to do about these key types, so ignore them
        printf("Unsupported key type %s\n", lua_typename(s, keyType));
        lua_pop(s, 1);
      }
    }

    std::sort(keys.begin(), keys.end());
    for(const std::string& key : keys) {
      _csv(first);
      _quoted(key);
      *mBuffer += " = ";
      lua_getfield(s, -1, key.c_str());
      _serialize();
    }

    --mDepth;
    _newline(nullptr);
    *mBuffer += "}";
    //Pop off table
    lua_pop(s, 1);
  }

  void Serializer::_serializeValue() {
    int type = lua_type(mS, -1);
    std::string& buffer = *mBuffer;
    switch(type) {
      case LUA_TNUMBER: {
        double num = static_cast<double>(lua_tonumber(mS, -1));
        double rounded = _round(num);
        //If rounded is within an epsilon of num, write as int, otherwise decimal
        if(std::abs(_roundInt(num) - rounded) < mEpsilon)
          buffer += std::to_string(static_cast<int>(rounded));
        else
          buffer += std::to_string(rounded);
        break;
      }
      case LUA_TSTRING:
        _quoted(lua_tostring(mS, -1));
        break;
      case LUA_TBOOLEAN:
        buffer += lua_toboolean(mS, -1) ? "true" : "false";
        break;
      default:
        printf("Unsupported value type %s\n", lua_typename(mS, type));
      case LUA_TNIL:
        buffer += "nil";
        break;
    }
    //Pop value to serialize off of stack
    lua_pop(mS, 1);
  }

  bool Serializer::_isSupportedKeyType(int type) {
    return type == LUA_TNUMBER || type == LUA_TSTRING;
  }

  void Serializer::_newline(const char* pre) {
    if(pre)
      *mBuffer += pre;
    *mBuffer += mNewline;
    for(int i = 0; i < mDepth; ++i)
      *mBuffer += mTab;
  }

  void Serializer::_csv(bool& first) {
    if(first)
      _newline(nullptr);
    else
      _newline(",");
    first = false;
  }

  void Serializer::_quoted(const std::string& str) {
    *mBuffer += "\"";
    *mBuffer += str;
    *mBuffer += "\"";
  }

  double Serializer::_round(double d) {
    //Truncate round
    int64_t i = static_cast<int64_t>(d*static_cast<double>(mRoundDigits));
    return static_cast<double>(i)/static_cast<double>(mRoundDigits);
  }

  double Serializer::_roundInt(double d) {
    return static_cast<double>(static_cast<int64_t>(d));
  }
}
