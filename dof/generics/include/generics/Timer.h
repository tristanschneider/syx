#pragma once

namespace gnx::time {
  template<class T>
  struct TimerT {
    void start(T targetTime) {
      countdown = targetTime;
    }

    //Tick until countdown is elapsed then emit true once
    bool tick() {
      if(countdown > 0) {
        if(--countdown == 0) {
          return true;
        }
      }
      return false;
    }

    T countdown{};
  };
  struct Timer : TimerT<size_t> {};
}