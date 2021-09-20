#pragma once

//A no-op lock that can be used in templates to opt-out of thread safety
struct NoLock {
public:
  void lock() {}
  bool try_lock() { return true; }
  void unlock() {}
};