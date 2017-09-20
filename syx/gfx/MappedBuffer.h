#pragma once

template<typename T>
struct DefaultHandlePred {
  Handle operator()(const T& obj) const {
    return obj.getHandle();
  }
};

template<typename T>
struct PtrHandlePred {
  Handle operator()(const T& obj) const {
    return obj->getHandle();
  }
};

//Store a linear buffer of T intended for fast iteration but still allowing for efficient lookups by handle
template<typename T, typename HandlePred = DefaultHandlePred<T>>
class MappedBuffer {
public:
  MappedBuffer() = default;
  MappedBuffer(MappedBuffer&&) = default;
  MappedBuffer& operator=(MappedBuffer&&) = default;

  T& pushBack(T& item) {
    mHandleToIndex[mGetHandle(item)] = mBuffer.size();
    mBuffer.emplace_back(std::move(item));
    return mBuffer.back();
  }

  void erase(Handle item) {
    auto it = mHandleToIndex.find(item);
    if(it != mHandleToIndex.end()) {
      if(mBuffer.size() > 1) {
        T& back = mBuffer.back();
        Handle swappedHandle = mGetHandle(back);
        mBuffer[it->second] = std::move(back);
        mHandleToIndex[swappedHandle] = it->second;
      }
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

  const std::vector<T>& getBuffer() const {
    return mBuffer;
  }

  T* get(Handle handle) {
    auto it = mHandleToIndex.find(handle);
    if(it != mHandleToIndex.end()) {
      return &mBuffer[it->second];
    }
    return nullptr;
  }

  const T* get(Handle handle) const {
    auto it = mHandleToIndex.find(handle);
    if(it != mHandleToIndex.end()) {
      return &mBuffer[it->second];
    }
    return nullptr;
  }

private:
  std::vector<T> mBuffer;
  std::unordered_map<Handle, size_t> mHandleToIndex;
  HandlePred mGetHandle;
};