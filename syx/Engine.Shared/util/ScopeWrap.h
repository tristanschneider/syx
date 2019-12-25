#pragma once
//Wraps the calling of two functions to push and pop some state

#define ScopeWrapType(state, pushFunc, popFunc) ScopeWrap<state, &state::pushFunc, &state::popFunc>

template<typename T, void(T::*PushState)(), void(T::*PopState)()>
class ScopeWrap {
public:
  using ThisType = ScopeWrap<T, PushState, PopState>;

  ScopeWrap(T& obj)
    : mObj(&obj) {
    _pushState();
  }

  ScopeWrap(ThisType&& rhs) {
    mObj = rhs.mObj;
    rhs.mObj = nullptr;
  }

  ~ScopeWrap() {
    _popState();
  }

  //Primary use case is on the stack, so copying or moving doesn't make sense
  ScopeWrap(const ThisType&) = delete;
  ThisType& operator=(const ThisType&) = delete;
  ThisType& operator=(ThisType&&) = delete;

private:
  void _pushState() {
    (mObj->*PushState)();
  }

  void _popState() {
    if(mObj)
      (mObj->*PopState)();
  }

  T* mObj;
};