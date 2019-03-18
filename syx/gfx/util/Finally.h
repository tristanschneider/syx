#pragma once

template<class Callable>
class FinalAction {
public:
  template<class Callable>
  FinalAction(Callable&& action)
    : mAction(std::move(action))
    , mIsValid(true) {
  }

  ~FinalAction() {
    if(mIsValid)
      mAction();
  }

  void cancel() {
    mIsValid = false;
  }

  FinalAction(const FinalAction&) = delete;
  FinalAction(FinalAction&& rhs)
    : mAction(std::move(rhs.mAction))
    , mIsValid(true) {
    rhs.mIsValid = false;
  }

  FinalAction& operator=(const FinalAction&) = delete;
  FinalAction& operator=(FinalAction&&) = delete;

private:
  Callable mAction;
  bool mIsValid;
};

template<class Callable>
FinalAction<Callable> finally(Callable&& callable) {
  return FinalAction<Callable>(std::move(callable));
}