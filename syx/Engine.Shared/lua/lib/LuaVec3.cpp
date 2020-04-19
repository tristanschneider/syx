#include "Precompile.h"
#include "lua/lib/LuaVec3.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "lua/LuaNode.h"

namespace Lua {
  const char* Vec3::CLASS_NAME = "Vec3";

  Vec3::Vec3(float x, float y, float z, float w)
    : mV(x, y, z, w) {
  }

  void Vec3::openLib(lua_State* l) {
    luaL_Reg statics[] = {
      { "new3", new3 },
      { "newSplat", newSplat },
      { "new", newDefault },
      { "unitX", unitX },
      { "unitY", unitY },
      { "unitZ", unitZ },
      { "zero", zero },
      { "one", one },
      { nullptr, nullptr }
    };
    luaL_Reg members[] = {
      { "len", length },
      { "len2", length2 },
      { "normalized", normalized },
      { "dist", dist },
      { "dist2", dist2 },
      { "set", set },
      { "copy", copy },
      { "add", add },
      { "sub", sub },
      { "neg", neg },
      { "cross", cross },
      { "dot", dot },
      { "divScalar", divScalar },
      { "mulVec", mulVec },
      { "mulScalar", mulScalar },
      { "projOnto", projOnto },
      { "recip", recip },
      { "getBasis", getBasis },
      { "lerp", lerp },
      { "__tostring", toString },
      { "__serialize", serialize },
      { "__typeNode", typeNode },
      { "__index", index },
      { "__newindex", newIndex },
      { "__eq", equality },
      { nullptr, nullptr }
    };
    Util::registerClass(l, statics, members, CLASS_NAME);
  }

  int Vec3::construct(lua_State* l, float x, float y, float z, float w) {
    void* data = lua_newuserdata(l, sizeof(Vec3));
    new (data) Vec3(x, y, z, w);
    luaL_setmetatable(l, CLASS_NAME);
    return 1;
  }

  int Vec3::construct(lua_State* l, const Syx::Vec3& v) {
    return construct(l, v.x, v.y, v.z, v.w);
  }

  int Vec3::new3(lua_State* l) {
    return construct(l, _getValue(l, 1), _getValue(l, 2), _getValue(l, 3), 0.0f);
  }

  int Vec3::newSplat(lua_State* l) {
    float s = _getValue(l, 1);
    return construct(l, s, s, s, 0.0f);
  }

  int Vec3::newDefault(lua_State* l) {
    return construct(l, 0, 0, 0, 0);
  }

  int Vec3::unitX(lua_State* l) {
    return construct(l, 1, 0, 0, 0);
  }

  int Vec3::unitY(lua_State* l) {
    return construct(l, 0, 1, 0, 0);
  }

  int Vec3::unitZ(lua_State* l) {
    return construct(l, 0, 0, 1, 0);
  }

  int Vec3::zero(lua_State* l) {
    return construct(l, 0, 0, 0, 0);
  }

  int Vec3::one(lua_State* l) {
    return construct(l, 1, 1, 1, 0);
  }

  int Vec3::length(lua_State* l) {
    lua_pushnumber(l, _getVec(l, 1).length());
    return 1;
  }

  int Vec3::length2(lua_State* l) {
    lua_pushnumber(l, _getVec(l, 1).length2());
    return 1;
  }

  int Vec3::normalized(lua_State* l) {
    return construct(l, _getVec(l, 1).normalized());
  }

  int Vec3::dist(lua_State* l) {
    lua_pushnumber(l, _getVec(l, 1).distance(_getVec(l, 2)));
    return 1;
  }

  int Vec3::dist2(lua_State* l) {
    lua_pushnumber(l, _getVec(l, 1).distance2(_getVec(l, 2)));
    return 1;
  }

  int Vec3::set(lua_State* l) {
    _getVec(l, 1) = Syx::Vec3(_getValue(l, 2), _getValue(l, 3), _getValue(l, 4));
    return 0;
  }

  int Vec3::copy(lua_State* l) {
    return construct(l, _getVec(l, 1));
  }

  int Vec3::add(lua_State* l) {
    return construct(l, _getVec(l, 1) + _getVec(l, 2));
  }

  int Vec3::sub(lua_State* l) {
    return construct(l, _getVec(l, 1) - _getVec(l, 2));
  }

  int Vec3::neg(lua_State* l) {
    return construct(l, -_getVec(l, 1));
  }

  int Vec3::cross(lua_State* l) {
    return construct(l, _getVec(l, 1).cross(_getVec(l, 2)));
  }

  int Vec3::dot(lua_State* l) {
    lua_pushnumber(l, _getVec(l, 1).dot(_getVec(l, 2)));
    return 1;
  }

  int Vec3::divScalar(lua_State* l) {
    return construct(l, Syx::Vec3::safeDivide(_getVec(l, 1), _getValue(l, 2)));
  }

  int Vec3::mulVec(lua_State* l) {
    return construct(l, Syx::Vec3::scale(_getVec(l, 1), _getVec(l, 2)));
  }

  int Vec3::mulScalar(lua_State* l) {
    return construct(l, _getVec(l, 1)*_getValue(l, 2));
  }

  int Vec3::projOnto(lua_State* l) {
    return construct(l, Syx::Vec3::projVec(_getVec(l, 1), _getVec(l, 2)));
  }

  int Vec3::recip(lua_State* l) {
    return construct(l, _getVec(l, 1).reciprocal());
  }

  int Vec3::getBasis(lua_State* l) {
    Syx::Vec3 x, y;
    _getVec(l, 1).getBasis(x, y);
    return construct(l, x) + construct(l, y);
  }

  int Vec3::lerp(lua_State* l) {
    return construct(l, Syx::Vec3::lerp(_getVec(l, 1), _getVec(l, 2), _getValue(l, 3)));
  }

  int Vec3::index(lua_State* l) {
    return Util::intIndexOverload(l, getIndex);
  }

  int Vec3::newIndex(lua_State* l) {
    return Util::intNewIndexOverload(l, setIndex);
  }

  int Vec3::getIndex(lua_State* l) {
    lua_pushnumber(l, _getVec(l, 1)[_getIndex(l, 2) - 1]);
    return 1;
  }

  int Vec3::setIndex(lua_State* l) {
    _getVec(l, 1)[_getIndex(l, 2) - 1] = _getValue(l, 3);
    return 0;
  }

  int Vec3::equality(lua_State* l) {
    lua_pushboolean(l, _getVec(l, 1) == _getVec(l, 2));
    return 1;
  }

  int Vec3::toString(lua_State* l) {
    const Syx::Vec3& v = _getVec(l, 1);
    std::string str = CLASS_NAME;
    str += "(" +
      std::to_string(v.x) + ", " +
      std::to_string(v.y) + ", " +
      std::to_string(v.z) + ")";
    lua_pushlstring(l, str.c_str(), str.size());
    return 1;
  }

  int Vec3::serialize(lua_State* l) {
    const Syx::Vec3& v = _getVec(l, 1);
    std::string str;
    str.reserve(50);
    str = CLASS_NAME;
    str += ".new3(";
    str += std::to_string(v.x);
    str += ", ";
    str += std::to_string(v.y);
    str += ", ";
    str += std::to_string(v.z);
    str += ")";
    lua_pushstring(l, str.c_str());
    return 1;
  }

  int Vec3::typeNode(lua_State* l) {
    //Don't need it, but just make sure it's valid
    _getVec(l, 1);
    lua_pushlightuserdata(l, const_cast<Lua::Vec3Node*>(&Lua::Vec3Node::singleton()));
    return 1;
  }

  Syx::Vec3& Vec3::_getVec(lua_State* l, int i) {
    return static_cast<Vec3*>(luaL_checkudata(l, i, CLASS_NAME))->mV;
  }

  int Vec3::_getIndex(lua_State* l, int i) {
    int index = static_cast<int>(luaL_checkinteger(l, i));
    luaL_argcheck(l, index >= 1 && index <= 3, i, "Out of bounds");
    return index;
  }

  float Vec3::_getValue(lua_State* l, int i) {
    return static_cast<float>(luaL_checknumber(l, i));
  }
}