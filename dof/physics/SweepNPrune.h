#pragma once

struct SweepElement {
  static constexpr size_t START_BIT = size_t(1) << (sizeof(size_t)*8 - 1);

  static SweepElement createBegin(float boundary, size_t value) {
    return { boundary, value | START_BIT };
  }

  static SweepElement createEnd(float boundary, size_t value) {
    return { boundary, value };
  }

  bool isStart() const {
    return mValue & START_BIT;
  }

  bool isEnd() const {
    return !isStart();
  }

  size_t getValue() const {
    return mValue & (~START_BIT);
  }

  float mBoundary{};
  size_t mValue{};
};

struct Sweep2D {
  std::vector<SweepElement> mX, mY;
  std::vector<size_t> mGained, mLost, mContaining;
  //Hack to be able to look up a given boundary by key. Probably a more elegant way to do this
  struct Bounds {
    float mMinX{};
    float mMaxX{};
    float mMinY{};
    float mMaxY{};
  };
  std::unordered_map<size_t, Bounds> mKeyToBoundaries;
};

struct SweepCollisionPair {
  bool operator==(const SweepCollisionPair& r) {
    return mA == r.mA && mB == r.mB;
  }

  size_t mA{};
  size_t mB{};
};

template<>
struct std::hash<SweepCollisionPair> {
  std::size_t operator()(const SweepCollisionPair& s) const noexcept {
    std::hash<size_t> h;
    //cppreference hash combine example
    return h(s.mA) ^ (h(s.mB) << 1);
  }
};

struct SweepNPrune {
  static void eraseRange(Sweep2D& sweep,
    const float* oldBoundaryMinX,
    const float* oldBoundaryMinY,
    const size_t* keys,
    std::vector<SweepCollisionPair>& lostPairs,
    size_t count);

  static void insertRange(Sweep2D& sweep,
    const float* newBoundaryMinX,
    const float* newBoundaryMinY,
    const float* newBoundaryMaxX,
    const float* newBoundaryMaxY,
    const size_t* keys,
    std::vector<SweepCollisionPair>& newPairs,
    size_t count);

  static void reinsertRange(Sweep2D& sweep,
    const float* prevBoundaryMinX,
    const float* prevBoundaryMinY,
    const float* newBoundaryMinX,
    const float* newBoundaryMinY,
    const float* newBoundaryMaxX,
    const float* newBoundaryMaxY,
    const size_t* keys,
    std::vector<SweepCollisionPair>& newPairs,
    std::vector<SweepCollisionPair>& removedPairs,
    size_t count);
};