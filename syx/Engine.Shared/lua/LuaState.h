#pragma once

struct lua_State;

namespace Lua {
  //Object that ownes a lua_State, while virtual to allow the implementation to contain other lifetimes that may be stored in the lua_State
  struct IState {
    virtual ~IState() = default;
    //This is expected to always be non-null, but provided as a pointer since that's what all lua functions expect
    virtual operator lua_State*() = 0;
  };

  class State : public IState {
  public:
    State();
    State(State&& s);
    ~State();

    State& operator=(const State&) = delete;
    operator lua_State*() override;
    lua_State* get();

  private:
    lua_State* mState;
  };
}