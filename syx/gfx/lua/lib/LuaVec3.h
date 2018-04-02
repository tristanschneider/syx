#pragma once
#include <SyxVec3.h>

struct lua_State;

namespace Lua {
  class Quat;

  class Vec3 {
  public:
    friend class Quat;

    Vec3(float x, float y, float z, float w);

    static void openLib(lua_State* l);
    static int construct(lua_State* l, float x, float y, float z, float w);
    static int construct(lua_State* l, Syx::Vec3& v);
    static int new3(lua_State* l);
    static int newSplat(lua_State* l);
    static int newDefault(lua_State* l);
    static int unitX(lua_State* l);
    static int unitY(lua_State* l);
    static int unitZ(lua_State* l);
    static int zero(lua_State* l);
    static int one(lua_State* l);
    static int length(lua_State* l);
    static int length2(lua_State* l);
    static int normalized(lua_State* l);
    static int dist(lua_State* l);
    static int dist2(lua_State* l);
    static int set(lua_State* l);
    static int copy(lua_State* l);
    static int add(lua_State* l);
    static int sub(lua_State* l);
    static int neg(lua_State* l);
    static int cross(lua_State* l);
    static int dot(lua_State* l);
    static int divScalar(lua_State* l);
    static int mulVec(lua_State* l);
    static int mulScalar(lua_State* l);
    static int projOnto(lua_State* l);
    static int recip(lua_State* l);
    static int getBasis(lua_State* l);
    static int lerp(lua_State* l);
    static int index(lua_State* l);
    static int newIndex(lua_State* l);
    static int toString(lua_State* l);
    static int getIndex(lua_State* l);
    static int setIndex(lua_State* l);
    static int equality(lua_State* l);

    static Syx::Vec3& _getVec(lua_State* l, int i);

  private:
    static const char* CLASS_NAME;

    static int _getIndex(lua_State* l, int i);
    static float _getValue(lua_State* l, int i);

    Syx::Vec3 mV;
  };
}