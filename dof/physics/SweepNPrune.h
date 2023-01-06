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
};

struct SweepCollisionPair {
  size_t mA{};
  size_t mB{};
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