#pragma once

struct lua_State;

namespace Lua {
  class AssetRepo {
  public:
    static void openLib(lua_State* l);

    static int getOrLoadAsset(lua_State* l);

  private:
    static const char* CLASS_NAME;
  };
};