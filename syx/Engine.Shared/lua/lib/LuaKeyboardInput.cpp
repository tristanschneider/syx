#include "Precompile.h"
#include "lua/lib/LuaKeyboardInput.h"

#include "lua/lib/LuaVec3.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "provider/SystemProvider.h"
#include "system/KeyboardInput.h"
#include "system/LuaGameSystem.h"

namespace Lua {
  const char* CLASS_NAME = "Keyboard";

  void KeyboardInput::openLib(lua_State* l) {
    luaL_Reg statics[] = {
      { "getKeyState", getKeyState },
      { "up", up },
      { "down", down },
      { "triggered", triggered },
      { "released", released },
      { "getMousePos", getMousePos },
      { "getMouseDetla", getMouseDelta },
      { "getWheelDelta", getWheelDelta },
      { nullptr, nullptr }
    };
    luaL_Reg members[] = {
      { nullptr, nullptr }
    };
    Util::registerClass(l, statics, members, CLASS_NAME);
  }

  int KeyboardInput::getKeyState(lua_State* l) {
    const char* key = luaL_checkstring(l, 1);
     lua_pushinteger(l, static_cast<lua_Integer>(_getInput(l).getKeyState(key)));
     return 1;
  }

  int KeyboardInput::up(lua_State* l) {
    lua_pushinteger(l, static_cast<lua_Integer>(KeyState::Up));
    return 1;
  }

  int KeyboardInput::down(lua_State* l) {
    lua_pushinteger(l, static_cast<lua_Integer>(KeyState::Down));
    return 1;
  }

  int KeyboardInput::triggered(lua_State* l) {
    lua_pushinteger(l, static_cast<lua_Integer>(KeyState::Triggered));
    return 1;
  }

  int KeyboardInput::released(lua_State* l) {
    lua_pushinteger(l, static_cast<lua_Integer>(KeyState::Released));
    return 1;
  }

  int KeyboardInput::getMousePos(lua_State* l) {
    const Syx::Vec2& result = _getInput(l).getMousePos();
    return Lua::Vec3::construct(l, result.x, result.y, 0.0f, 0.0f);
  }

  int KeyboardInput::getMouseDelta(lua_State* l) {
    const Syx::Vec2& result = _getInput(l).getMouseDelta();
    return Lua::Vec3::construct(l, result.x, result.y, 0.0f, 0.0f);
  }

  int KeyboardInput::getWheelDelta(lua_State* l) {
    lua_pushnumber(l, static_cast<lua_Number>(_getInput(l).getWheelDelta()));
    return 1;
  }

  ::KeyboardInput& KeyboardInput::_getInput(lua_State* l) {
    return *LuaGameSystem::check(l).getSystemProvider().getSystem<::KeyboardInput>();
  }
}