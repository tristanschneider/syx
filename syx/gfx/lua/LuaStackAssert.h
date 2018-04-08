#pragma once
//Object to assert that the lua stack is the same size between construction and destruction

struct lua_State;

namespace Lua {
  class State;

  class StackAssert {
  public:
    StackAssert(lua_State* lua, int expectedDiff = 0);
    ~StackAssert();

  private:
    int mExpectedStack;
    lua_State& mState;
  };
}