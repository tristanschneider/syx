#pragma once

struct lua_State;

namespace Lua {
  class Key {
  public:
    Key(const char* key);
    Key(int key = 0);

    int push(lua_State* l) const;
    bool readFromLua(lua_State* l, int index);

  private:
    std::string mStr;
    int mIndex;
  };
}