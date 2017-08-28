#pragma once

#define InvalidHandle static_cast<size_t>(-1)

typedef size_t Handle;

class HandleGen {
public:
  HandleGen() {
    Reset();
  }

  void Reset() {
    mNewKey = 0;
  }

  Handle next() {
    Handle result = mNewKey++;
    if(mNewKey == InvalidHandle)
      ++mNewKey;
    return result;
  }

private:
  Handle mNewKey;
};