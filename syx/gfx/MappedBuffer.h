#pragma once

//Store a linear buffer of T intended for fast iteration but still allowing for efficient lookups by handle
template<typename T>
class MappedBuffer {
public:
  MappedBuffer() = default;
  MappedBuffer(MappedBuffer&&) = default;
  MappedBuffer& operator=(MappedBuffer&&) = default;

  T& pushBack(T& item, Handle handle) {
    mHandleToIndex[handle] = mBuffer.size();
    mBuffer.emplace_back(std::move(item));
    return mBuffer.back();
  }

  void erase(Handle item) {
    auto it = mHandleToIndex.find(item);
    if(it != mHandleToIndex.end()) {
      if(mBuffer.size() > 1)
        mBuffer[it->second] = std::move(mBuffer.back());
      mBuffer.pop_back();
      mHandleToIndex.erase(it);
    }
  }

  void clear() {
    mBuffer.clear();
    mHandleToIndex.clear();
  }

  std::vector<T>& getBuffer() {
    return mBuffer;
  }

  T* get(Handle handle) {
    auto it = mHandleToIndex.find(handle);
    if(it != mHandleToIndex.end()) {
      return &mBuffer[it->second];
    }
    return nullptr;
  }

private:
  std::vector<T> mBuffer;
  std::unordered_map<Handle, size_t> mHandleToIndex;
};