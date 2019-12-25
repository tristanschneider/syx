#pragma once

struct lua_State;

namespace Lua {
  class State {
  public:
    State();
    State(State&& s);
    ~State();

    State& operator=(const State&) = delete;
    operator lua_State*();
    lua_State* get();

  private:
    lua_State* mState;
  };
}