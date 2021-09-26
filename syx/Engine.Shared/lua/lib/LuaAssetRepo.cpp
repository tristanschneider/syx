#include "Precompile.h"
#include "lua/lib/LuaAssetRepo.h"

#include "asset/Asset.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaUtil.h"

#include <lua.hpp>

#include "event/AssetEvents.h"
#include "event/EventBuffer.h"
#include "provider/MessageQueueProvider.h"

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
    const AssetInfo info(assetName);
    Lua::checkGameContext(l).getMessageProvider().getMessageQueue()->push(GetAssetRequest(info));
    lua_pushlightuserdata(l, reinterpret_cast<void*>(info.mId));
    //TODO: Do something to indicate shared pointer ownership? Right now this relies on the repo retaining it
    //TODO: indicate null if asset doesn't exist? Can't be known immediately
    return 1;
  }
}