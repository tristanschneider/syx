#pragma once
//Object to assert that the lua stack is the same size between construction and destruction

struct lua_State;

namespace Lua {
  class State;

  class StackAssert {
  public:
    StackAssert(lua_State* lua);
    ~StackAssert();

  private:
    int mStartStack;
    lua_State& mState;
  };
}