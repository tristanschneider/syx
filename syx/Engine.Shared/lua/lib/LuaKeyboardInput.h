#pragma once

struct lua_State;
class KeyboardInput;

namespace Lua {
  class KeyboardInput {
  public:
    static void openLib(lua_State* l);

    // number getKeyState(string key)
    static int getKeyState(lua_State* l);
    // 4 below are: number up()
    static int up(lua_State* l);
    static int down(lua_State* l);
    static int triggered(lua_State* l);
    static int released(lua_State* l);

    // Vec3 getMousePos()
    static int getMousePos(lua_State* l);
    // Vec3 getMouseDelta()
    static int getMouseDelta(lua_State* l);
    // number getWheelDelta()
    static int getWheelDelta(lua_State* l);
  };
};
