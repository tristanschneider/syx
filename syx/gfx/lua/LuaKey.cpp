#include "Precompile.h"
#include "lua/LuaKey.h"

#include <lua.hpp>

namespace Lua {
  Key::Key(const char* key)
    : mStr(key) {
    mHash = std::hash<std::string>()(mStr);
  }

  Key::Key(int key)
    : mIndex(key) {
  }

  bool Key::operator==(const Key& key) const {
    return mHash == key.mHash;
  }

  bool Key::operator!=(const Key& key) const {
    return !(*this == key);
  }

  int Key::push(lua_State* l) const {
    if(!mStr.empty())
      lua_pushstring(l, mStr.c_str());
    else
      lua_pushinteger(l, static_cast<lua_Integer>(mIndex));
    return 1;
  }

  bool Key::readFromLua(lua_State* l, int index) {
    if(lua_isinteger(l, index)) {
      *this = Key(static_cast<int>(lua_tointeger(l, index)));
      return true;
    }
    if(lua_isstring(l, index)) {
      *this = Key(lua_tostring(l, index));
      return true;
    }
    return false;
  }

  size_t Key::getHash() const {
    //This is either the actual hash or the index, both of which work for comparison
    return mHash;
  }
}