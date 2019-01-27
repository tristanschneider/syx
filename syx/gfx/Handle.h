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

  //Prevent this handle from being generated from a future call to next
  void blacklistHandle(Handle used) {
    //Handles are generated in ascending order, so put next key above the blacklisted key.
    //Doesn't handle overflow, but neither does next
    Handle curValue = mNewKey;
    if(curValue < used)
      mNewKey.fetch_add(used - curValue);
  }

private:
  std::atomic<Handle> mNewKey;
};