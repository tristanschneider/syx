#pragma once
//Object to assert that the lua stack is the same size between construction and destruction

namespace Lua {
  class State;

  class StackAssert {
  public:
    StackAssert(State& state);
    ~StackAssert();

  private:
    int mStartStack;
    State& mState;
  };
}