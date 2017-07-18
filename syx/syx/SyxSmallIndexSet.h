#pragma once

//Set for small range of indices with static memory, but will fall back on allocated memory if it has to
template<size_t StaticBytes>
class SmallIndexSet {
public:
  SmallIndexSet() {
    Clear();
  }

  //Returns previous value at index
  bool Insert(size_t i) {
    size_t staticBits = StaticBytes*8;
    if(i < staticBits) {
      return InsertBuffer(i, mStatic);
    }

    i -= staticBits;
    if(mDynamic.size() <= i) {
      mDynamic.resize(i + 1, 0);
    }
    return InsertBuffer(i, mDynamic.data());
  }

  //Caller is responsible for not calling with nonsense index
  //Returns previous value at index
  bool Remove(size_t i) {
    size_t staticBits = StaticBytes*8;
    if(i < staticBits) {
      return RemoveBuffer(i, mStatic);
    }

    i -= staticBits;
    return RemoveBuffer(i, mDynamic);
  }

  bool Contains(size_t i) {
    size_t staticBits = StaticBytes*8;
    if(i < staticBits) {
      return ContainsBuffer(i, mStatic);
    }

    i -= staticBits;
    if(i >= mDynamic.size())
      return false;
    return ContainsBuffer(i, mDynamic);
  }

  void Clear() {
    std::memset(mStatic, 0, StaticBytes);
    mDynamic.clear();
  }

private:
  bool InsertBuffer(size_t i, char* buffer) {
    char& iByte = buffer[i/8];
    char mask = 1 << (i % 8);
    bool result = (iByte & mask) != 0;
    iByte |= mask;
    return result;
  }

  bool RemoveBuffer(size_t i, char* buffer) {
    char& iByte = buffer[i/8];
    char mask = 1 << (i % 8);
    bool result = (iByte & mask) != 0;
    iByte &= ~mask;
    return result;
  }

  bool ContainsBuffer(size_t i, char* buffer) {
    char& iByte = buffer[i/8];
    char mask = 1 << (i % 8);
    return (iByte & mask) != 0;
  }

  char mStatic[StaticBytes];
  std::vector<char> mDynamic;
};