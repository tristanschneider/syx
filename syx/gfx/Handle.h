#pragma once

#define InvalidHandle static_cast<size_t>(-1)

typedef size_t Handle;

class HandleGen {
public:
  HandleGen() {
    reset();
  }

  void reset() {
    mNewKey = 0;
  }

  Handle next() {
    Handle inc = 1;
    Handle result = mNewKey.fetch_add(inc) + inc;
    if(result == InvalidHandle)
      result = mNewKey.fetch_add(inc) + inc;
    return result;
  }

private:
  std::atomic<Handle> mNewKey;
};