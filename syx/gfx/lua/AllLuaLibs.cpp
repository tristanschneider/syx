#include "Precompile.h"
#include "lua/AllLuaLibs.h"

#include "lua/lib/LuaNumArray.h"
#include "lua/lib/LuaNumVec.h"
#include "lua/lib/LuaVec3.h"
#include "lua/lib/LuaQuat.h"

namespace Lua {
  void AllLuaLibs::open(lua_State* l) {
    NumArray::openLib(l);
    NumVec::openLib(l);
    Vec3::openLib(l);
    Quat::openLib(l);
  }
}