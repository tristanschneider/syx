#include "Precompile.h"
#include "lua/lib/LuaQuat.h"
#include "lua/lib/LuaVec3.h"
#include "lua/LuaUtil.h"
#include <SyxVec3.h>
#include <lua.hpp>

namespace Lua {
  const char* Quat::CLASS_NAME = "Quat";

  Quat::Quat(float x, float y, float z, float w)
    : mQ(x, y, z, w) {
  }

  void Quat::openLib(lua_State* l) {
    luaL_Reg statics[] = {
      { "newAxisAngle", newAxisAngle },
      { "newFromTo", newFromTo },
      { "newLookAt", newLookAt },
      { nullptr, nullptr }
    };
    luaL_Reg members[] = {
      { "inverse", inverse },
      { "normalized", normalized },
      { "mul", mul },
      { "mulScalar", mulScalar },
      { "rot", rot },
      { "getBasis", getBasis },
      { "slerp", slerp },
      { "__tostring", toString },
      { "__eq", equality },
      { nullptr, nullptr }
    };
    Util::registerClass(l, statics, members, CLASS_NAME, true, false);
  }

  int Quat::construct(lua_State* l, float x, float y, float z, float w) {
    void* data = lua_newuserdata(l, sizeof(Quat));
    Quat* q = new (data) Quat(x, y, z, w);
    luaL_setmetatable(l, CLASS_NAME);
    return 1;
  }

  int Quat::construct(lua_State* l, const Syx::Quat& q) {
    return construct(l, q.mV.x, q.mV.y, q.mV.z, q.mV.w);
  }

  int Quat::newAxisAngle(lua_State* l) {
    return construct(l, Syx::Quat::axisAngle(_getVec(l, 1).normalized(), _getValue(l, 2)));
  }

  int Quat::newFromTo(lua_State* l) {
    return construct(l, Syx::Quat::getRotation(_getVec(l, 1).normalized(), _getVec(l, 2).normalized()));
  }

  int Quat::newLookAt(lua_State* l) {
    const Syx::Vec3& forward = _getVec(l, 1);
    Syx::Vec3 up = Syx::Vec3::UnitY;
    if(lua_isuserdata(l, 2)) {
      up = _getVec(l, 2);
    }
    //Correct up to be orthogonal to forward
    up = forward.cross(up.cross(forward)).safeNormalized();
    if(up.length2() == 0.0f)
      up = forward.getOrthogonal();
    return construct(l, Syx::Quat::lookAt(forward, up));
  }

  int Quat::inverse(lua_State* l) {
    return construct(l, _getQuat(l, 1).inversed());
  }

  int Quat::normalized(lua_State* l) {
    return construct(l, _getQuat(l, 1).normalized());
  }

  int Quat::mul(lua_State* l) {
    return construct(l, _getQuat(l, 1)*_getQuat(l, 2));
  }

  int Quat::mulScalar(lua_State* l) {
    return construct(l, _getQuat(l, 1)*_getValue(l, 2));
  }

  int Quat::rot(lua_State* l) {
    return Vec3::construct(l, _getQuat(l, 1)*_getVec(l, 2));
  }

  int Quat::getBasis(lua_State* l) {
    Syx::Vec3 x, y, z;
    const Syx::Quat& q = _getQuat(l, 1);
    x = q.getRight();
    y = q.getUp();
    z = q.getForward();
    return Vec3::construct(l, x) + Vec3::construct(l, y) + Vec3::construct(l, z);
  }

  int Quat::slerp(lua_State* l) {
    float t = _getValue(l, 3);
    t = std::max(0.0f, std::min(1.0f, t));
    return construct(l, Syx::Quat::slerp(_getQuat(l, 1), _getQuat(l, 2), t));
  }

  int Quat::toString(lua_State* l) {
    const Syx::Quat& q = _getQuat(l, 1);
    std::string str = CLASS_NAME;
    str += "(" +
      std::to_string(q.mV.x) + ", " +
      std::to_string(q.mV.y) + ", " +
      std::to_string(q.mV.z) + ", " +
      std::to_string(q.mV.w) + ")";
    lua_pushlstring(l, str.c_str(), str.size());
    return 1;
  }

  int Quat::equality(lua_State* l) {
    Syx::Quat& a = _getQuat(l, 1);
    Syx::Quat& b = _getQuat(l, 2);
    lua_pushboolean(l, a.mV == b.mV && a.mV.w == b.mV.w);
    return 1;
  }

  Syx::Quat& Quat::_getQuat(lua_State* l, int i) {
    return static_cast<Quat*>(luaL_checkudata(l, i, CLASS_NAME))->mQ;
  }

  Syx::Vec3& Quat::_getVec(lua_State* l, int i) {
    return static_cast<Vec3*>(luaL_checkudata(l, i, Vec3::CLASS_NAME))->mV;
  }

  float Quat::_getValue(lua_State* l, int i) {
    return static_cast<float>(luaL_checknumber(l, i));
  }
}