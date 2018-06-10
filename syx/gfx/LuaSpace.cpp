#include "Precompile.h"
#include "LuaSpace.h"

#include <lua.hpp>
#include "lua/LuaCache.h"
#include "lua/LuaUtil.h"
#include "system/LuaGameSystem.h"

const char* LuaSpace::CLASS_NAME = "Space";
const std::unique_ptr<Lua::Cache> LuaSpace::sCache = std::make_unique<Lua::Cache>("_lsc_", CLASS_NAME);

LuaSpace::LuaSpace(Handle id)
  : mId(id) {
}

LuaSpace::~LuaSpace() {
}

Handle LuaSpace::getId() const {
  return mId;
}

void LuaSpace::openLib(lua_State* l) {
  luaL_Reg statics[] = {
    { "get", get },
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    { nullptr, nullptr }
  };
  sCache->createCache(l);
  Lua::Util::registerClass(l, statics, members, CLASS_NAME, true);
}

int LuaSpace::get(lua_State* l) {
  const char* name = luaL_checkstring(l, 1);
  LuaGameSystem& game = LuaGameSystem::check(l);
  push(l, game.getSpace(Util::constHash(name)));
  return 1;
}

int LuaSpace::push(lua_State* l, LuaSpace& obj) {
  return sCache->push(l, &obj, obj.getId());
}

int LuaSpace::invalidate(lua_State* l, LuaSpace& obj) {
  return sCache->invalidate(l, obj.getId());
}

LuaSpace& LuaSpace::getObj(lua_State* l, int index) {
  return *static_cast<LuaSpace*>(sCache->checkParam(l, index));
}
