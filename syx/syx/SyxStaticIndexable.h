#pragma once
//Container that maintains values in contiguous buffer that may have holes.
//Index will always be valid with constant time insertion, removal, and lookups
//Meant for when a buffer of objects needs to be maintained that may occasionally have additions and removals,
//but will often be accessed somewhat randomly such that the contiguous memory can decrease cache misses.
//Also good for copying, as buffer and indices can be copied without issue, unlike the intrusive containers

namespace Syx {
  template<typename T>
  class StaticIndexable {
  public:
    StaticIndexable()
      : mSize(0) {}

    const T& operator[](size_t index) const {
      return mBuffer[index];
    }

    T& operator[](size_t index) {
      return mBuffer[index];
    }

    void Erase(size_t index) {
      mFreeIndices.push_back(index);
      --mSize;
    }

    void Clear() {
      mBuffer.clear();
      mFreeIndices.clear();
      mSize = 0;
    }

    size_t Push(const T& obj) {
      size_t index;
      if(!mFreeIndices.empty()) {
        index = mFreeIndices.back();
        mBuffer[index] = obj;
        mFreeIndices.pop_back();
      }
      else {
        index = mBuffer.size();
        mBuffer.push_back(obj);
      }
      ++mSize;
      return index;
    }

    void GetIndices(std::vector<size_t>& indices) {
      indices.clear();
      //Sort so we can iterate along this in parallel to skip free nodes
      std::sort(mFreeIndices.begin(), mFreeIndices.end());
      size_t found = 0;
      size_t freeListIndex = 0;
      size_t freeIndex = mBuffer.size();
      if(!mFreeIndices.empty())
        freeIndex = mFreeIndices[freeListIndex];

      for(size_t i = 0; i < mBuffer.size(); ++i) {
        //If this isn't a free node
        if(freeIndex != i) {
          indices.push_back(i);
          //Stop if we've already found all non-free nodes
          if(++found >= mSize)
            return;
        }
        if(freeIndex <= i && freeListIndex + 1 < mFreeIndices.size()) {
          ++freeListIndex;
          freeIndex = mFreeIndices[freeListIndex];
        }
      }
    }

    size_t Size() {
      return mSize;
    }

  private:
    std::vector<T> mBuffer;
    std::vector<size_t> mFreeIndices;
    size_t mSize;
  };
}