#pragma once

namespace Lua {
  class Cache;
};

struct lua_State;

class LuaSpace {
public:
  LuaSpace(Handle id);
  ~LuaSpace();

  Handle getId() const;

  static void openLib(lua_State* l);

  // Space get(string name)
  static int get(lua_State* l);

  static int push(lua_State* l, LuaSpace& obj);
  static int invalidate(lua_State* l, LuaSpace& obj);

  static LuaSpace& getObj(lua_State* l, int index);

private:
  static const char* CLASS_NAME;
  static const std::unique_ptr<Lua::Cache> sCache;

  Handle mId;
};