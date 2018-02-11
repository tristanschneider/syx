#pragma once
//Upon construction, creates a sandbox table and saves it as a global under the given id
//This sandbox can then be pushed onto the stack and 

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