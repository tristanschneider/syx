#include "Precompile.h"
#include "lua/lib/LuaAssetRepo.h"

#include "asset/Asset.h"
#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"
#include "lua/LuaUtil.h"

#include <lua.hpp>

namespace Lua {
  const char* AssetRepo::CLASS_NAME = "AssetRepo";

  void AssetRepo::openLib(lua_State* l) {
    luaL_Reg statics[] = {
      { "getOrLoadAsset", getOrLoadAsset },
      { nullptr, nullptr }
    };
    luaL_Reg members[] = {
      { nullptr, nullptr }
    };
    Util::registerClass(l, statics, members, CLASS_NAME);
  }

  int AssetRepo::getOrLoadAsset(lua_State* l) {
    const char* assetName = luaL_checkstring(l, 1);
    if(std::shared_ptr<Asset> asset = _getRepo(l).getAsset(AssetInfo(assetName))) {
      lua_pushlightuserdata(l, reinterpret_cast<void*>(asset->getInfo().mId));
      //TODO: Do something to indicate shared pointer ownership? Right now this relies on the repo retaining it
    }
    else
      lua_pushnil(l);
    return 1;
  }

  ::AssetRepo& AssetRepo::_getRepo(lua_State* l) {
    return LuaGameSystem::check(l).getAssetRepo();
  }
}