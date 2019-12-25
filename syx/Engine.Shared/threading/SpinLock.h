#pragma once

class SpinLock {
public:
  SpinLock()
    : mAcquired(false) {
  }

  void lock() {
    bool wasAcquired = false;
    //Keep trying until we change acquired from false to true
    while(!mAcquired.compare_exchange_weak(wasAcquired, true))
      wasAcquired = false;
  }

  bool try_lock() {
    bool wasAcquired = false;
    return mAcquired.compare_exchange_strong(wasAcquired, true);
  }

  void unlock() {
    bool wasAcquired = mAcquired;
    //If we're trying to unlock it should already be acquired
    assert(wasAcquired);
    mAcquired.compare_exchange_strong(wasAcquired, false);
  }

private:
  std::atomic_bool mAcquired;
};