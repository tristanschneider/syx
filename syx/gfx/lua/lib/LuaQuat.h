#pragma once
#include <SyxQuat.h>

struct lua_State;

namespace Syx {
  struct Vec3;
}

namespace Lua {
  class Vec3;

  class Quat {
  public:
    static const char* CLASS_NAME;

    Quat(float x, float y, float z, float w);

    static void openLib(lua_State* l);
    static int construct(lua_State* l, float x, float y, float z, float w);
    static int construct(lua_State* l, const Syx::Quat& q);

    static int newAxisAngle(lua_State* l);
    static int newFromTo(lua_State* l);
    static int newLookAt(lua_State* l);
    static int inverse(lua_State* l);
    static int normalized(lua_State* l);
    static int mul(lua_State* l);
    static int mulScalar(lua_State* l);
    static int rot(lua_State* l);
    static int getBasis(lua_State* l);
    static int slerp(lua_State* l);
    static int toString(lua_State* l);
    static int equality(lua_State* l);

    static Syx::Quat& _getQuat(lua_State* l, int i);

  private:
    static Syx::Vec3& _getVec(lua_State* l, int i);
    static float _getValue(lua_State* l, int i);

    Syx::Quat mQ;
  };
}