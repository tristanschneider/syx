#pragma once

//Set for small range of indices with static memory, but will fall back on allocated memory if it has to
template<size_t StaticBytes>
class SmallIndexSet {
public:
  SmallIndexSet() {
    clear();
  }

  //Returns previous value at index
  bool insert(size_t i) {
    size_t staticBits = StaticBytes*8;
    if(i < staticBits) {
      return _insertBuffer(i, mStatic);
    }

    i -= staticBits;
    if(mDynamic.size() <= i) {
      mDynamic.resize(i + 1, 0);
    }
    return _insertBuffer(i, mDynamic.data());
  }

  //Caller is responsible for not calling with nonsense index
  //Returns previous value at index
  bool remove(size_t i) {
    size_t staticBits = StaticBytes*8;
    if(i < staticBits) {
      return _removeBuffer(i, mStatic);
    }

    i -= staticBits;
    return _removeBuffer(i, mDynamic);
  }

  bool contains(size_t i) {
    size_t staticBits = StaticBytes*8;
    if(i < staticBits) {
      return _containsBuffer(i, mStatic);
    }

    i -= staticBits;
    if(i >= mDynamic.size())
      return false;
    return _containsBuffer(i, mDynamic);
  }

  void clear() {
    std::memset(mStatic, 0, StaticBytes);
    mDynamic.clear();
  }

private:
  bool _insertBuffer(size_t i, char* buffer) {
    char& iByte = buffer[i/8];
    char mask = 1 << (i % 8);
    bool result = (iByte & mask) != 0;
    iByte |= mask;
    return result;
  }

  bool _removeBuffer(size_t i, char* buffer) {
    char& iByte = buffer[i/8];
    char mask = 1 << (i % 8);
    bool result = (iByte & mask) != 0;
    iByte &= ~mask;
    return result;
  }

  bool _containsBuffer(size_t i, char* buffer) {
    char& iByte = buffer[i/8];
    char mask = 1 << (i % 8);
    return (iByte & mask) != 0;
  }

  char mStatic[StaticBytes];
  std::vector<char> mDynamic;
};