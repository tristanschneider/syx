#pragma once
#include "util/ScopeWrap.h"
//Upon construction, creates a sandbox table and saves it as a global under the given id
//This sandbox can then be used as an updvalue to replace _ENV making all global access contained within the sandbox

namespace Lua {
  class State;

  class Sandbox {
  public:
    Sandbox(State& state, const std::string& id);
    ~Sandbox();

    Sandbox(Sandbox&& rhs);
    Sandbox& operator=(Sandbox&& rhs);

    //States are stored in global table by id so they shouldn't be copied
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;

    //Push the lua state to the top of the stack
    //Also sets the upvalue of the chunk so any functions called with in it are sandboxed
    void push();
    void pop();

    using ScopedState = ScopeWrapType(Sandbox, push, pop);

  private:
    void _createSandbox();
    void _destroySandbox();
    //Set the created sandbox as the upvalue for the function on top of the stack
    void _setUpvalue();
    void _clear();

    std::string mId;
    State* mState;

    static const char* CHUNK_ID;
  };
}