#include "Precompile.h"
#include "lua/LuaUtil.h"

#include <lua.hpp>
#include "lua/LuaSerializer.h"
#include "lua/LuaState.h"

namespace Lua {
  void printTop(State& state) {
    Lua::Serializer s("\t", "\n", 1);
    std::string buff;
    s.serializeTop(state, buff);
    printf("%s\n", buff.c_str());
  }

  void printGlobal(State& state, const std::string& global) {
    Lua::Serializer s("\t", "\n", 1);
    std::string buff;
    s.serializeGlobal(state, global, buff);
    printf("%s\n", buff.c_str());
  }
}