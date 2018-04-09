#include "Precompile.h"
#include "lua/AllLuaLibs.h"

#include "component/Component.h"
#include "lua/lib/LuaNumArray.h"
#include "lua/lib/LuaNumVec.h"
#include "lua/lib/LuaVec3.h"
#include "lua/lib/LuaQuat.h"
#include "LuaGameObject.h"

namespace Lua {
  void AllLuaLibs::open(lua_State* l) {
    NumArray::openLib(l);
    NumVec::openLib(l);
    Vec3::openLib(l);
    Quat::openLib(l);
    LuaGameObject::openLib(l);
    Component::baseOpenLib(l);

    //Construct a temporary of each type to call opeenlibs
    const auto& ctors = Component::Registry::getConstructors();
    for(auto it = ctors.begin(); it != ctors.end(); ++it) {
      (*it)(0)->openLib(l);
    }
  }
}