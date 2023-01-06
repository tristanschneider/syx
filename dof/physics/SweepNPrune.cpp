#include "Precompile.h"
#include "SweepNPrune.h"

#include <cassert>

namespace {
  struct FindByBoundary {
    bool operator()(const SweepElement& l, float v) {
      return l.mBoundary < v;
    }
  };

  auto findLowerBoundaryFirstEqual(std::vector<SweepElement>& axis, float value) {
    auto it = std::lower_bound(axis.begin(), axis.end(), value, FindByBoundary{});
    return it;
  }

  auto findLowerBoundaryLastEqual(std::vector<SweepElement>& axis, float value) {
    auto it = std::lower_bound(axis.begin(), axis.end(), value, FindByBoundary{});
    while(it != axis.end() && it + 1 != axis.end() && (it + 1)->mBoundary == value) {
      ++it;
    }
    return it;
  }

  auto findKey(std::vector<SweepElement>& axis, size_t key, float value) {
    auto it = std::lower_bound(axis.begin(), axis.end(), value, FindByBoundary{});
    while(it != axis.end() && it->getValue() != key && it->mBoundary == value) {
      ++it;
    }
    assert(it == axis.end() || it->isStart());
    return it;
  }

  auto walkToEnd(std::vector<SweepElement>::iterator it, std::vector<size_t>& collect) {
    const size_t toFind = it->mValue;
    ++it;
    while(true) {
      if(it->mValue != toFind) {
        collect.push_back(it->getValue());
        ++it;
      }
      else {
        break;
      }
    }
    assert(it->isEnd());
    return it;
  }

  using SweepIt = std::vector<SweepElement>::iterator;
  using ShouldCollect = bool(SweepElement::*)() const;

  void moveBoundary(SweepIt boundary,
    SweepIt begin,
    SweepIt end,
    size_t key,
    float newBoundary,
    std::vector<size_t>& collectLower,
    std::vector<size_t>& collectHigher,
    ShouldCollect shouldCollectLower,
    ShouldCollect shouldCollectHigher) {
    if(begin == end) {
      return;
    }
    //move starts at the lower end of the boundary regardless of if it matched the key
    //Collect values traversed before finding the key but don't swap them
    bool keyFound = boundary != end && boundary->getValue() == key;
    //If new min is lower, swap down and log all gained elements along the way
    if(boundary == end || boundary->mBoundary > newBoundary) {
      --boundary;
      while(boundary != begin && boundary->mBoundary >= newBoundary) {
        if(boundary->getValue() != key && (*boundary.*shouldCollectLower)()) {
          collectLower.push_back(boundary->getValue());
        }
        if(keyFound) {
          std::swap(*boundary, *(boundary + 1));
        }
        else {
          keyFound = boundary->getValue() == key;
        }
        --boundary;
      }
    }
    else {
      //If new min is higher, swap up and log all lost elements along the way
      ++boundary;
      while(boundary != end && boundary->mBoundary <= newBoundary) {
        if(boundary->getValue() != key && (*boundary.*shouldCollectHigher)()) {
          collectHigher.push_back(boundary->getValue());
        }
        if(keyFound) {
          std::swap(*boundary, *(boundary - 1));
        }
        else {
          keyFound = boundary->getValue() == key;
        }
        ++boundary;
      }
    }
  }
};

void SweepNPrune::eraseRange(Sweep2D& sweep,
  const float* oldBoundaryMinX,
  const float* oldBoundaryMinY,
  const size_t* keys,
  std::vector<SweepCollisionPair>& lostPairs,
  size_t count) {
  for(size_t i = 0; i < count; ++i) {
    sweep.mLost.clear();
    auto findX = findKey(sweep.mX, keys[i], oldBoundaryMinX[i]);
    auto findY = findKey(sweep.mY, keys[i], oldBoundaryMinY[i]);
    if(findX == sweep.mX.end() || findY == sweep.mY.end()) {
      //Either both should be in the list or neither
      assert(findX == sweep.mX.end() && findY == sweep.mY.end());
      continue;
    }

    walkToEnd(findX, sweep.mLost);
    walkToEnd(findY, sweep.mLost);

    std::sort(sweep.mLost.begin(), sweep.mLost.end());
    sweep.mLost.erase(std::unique(sweep.mLost.begin(), sweep.mLost.end()), sweep.mLost.end());
    for(const size_t& loss : sweep.mLost) {
      lostPairs.push_back({ keys[i], loss });
    }
  }
}

void SweepNPrune::insertRange(Sweep2D& sweep,
  const float* newBoundaryMinX,
  const float* newBoundaryMinY,
  const float* newBoundaryMaxX,
  const float* newBoundaryMaxY,
  const size_t* keys,
  std::vector<SweepCollisionPair>& newPairs,
  size_t count) {
  for(size_t i = 0; i < count; ++i) {
    sweep.mGained.clear();

    auto findX = findLowerBoundaryFirstEqual(sweep.mX, newBoundaryMinX[i]);
    auto findY = findLowerBoundaryFirstEqual(sweep.mY, newBoundaryMinY[i]);
    auto newBegin = sweep.mX.insert(findX, SweepElement::createBegin(newBoundaryMinX[i], keys[i]));
    auto next = newBegin + 1;
    while(next != sweep.mX.end() && next->mBoundary < newBoundaryMaxX[i]) {
      sweep.mGained.push_back(next->getValue());
    }
    sweep.mX.insert(next, SweepElement::createEnd(newBoundaryMaxX[i], keys[i]));

    newBegin = sweep.mY.insert(findY, SweepElement::createBegin(newBoundaryMinY[i], keys[i]));
    next = newBegin + 1;
    while(next != sweep.mY.end() && next->mBoundary < newBoundaryMaxY[i]) {
      sweep.mGained.push_back(next->getValue());
    }
    sweep.mY.insert(next, SweepElement::createEnd(newBoundaryMaxY[i], keys[i]));

    //Duplicates indicate they were added on both axes, meaning they are collision pairs
    std::sort(sweep.mGained.begin(), sweep.mGained.end());
    for(size_t j = 0; j < sweep.mGained.size();) {
      if(j + 1 < sweep.mGained.size() && sweep.mGained[j] == sweep.mGained[j + 1]) {
        newPairs.push_back({ keys[j], sweep.mGained[j] });
      }
    }
  }
}

void SweepNPrune::reinsertRange(Sweep2D& sweep,
  const float* prevBoundaryMinX,
  const float* prevBoundaryMinY,
  const float* newBoundaryMinX,
  const float* newBoundaryMinY,
  const float* newBoundaryMaxX,
  const float* newBoundaryMaxY,
  const size_t* keys,
  std::vector<SweepCollisionPair>& newPairs,
  std::vector<SweepCollisionPair>& removedPairs,
  size_t count) {
  std::vector<size_t>& gained = sweep.mGained;
  std::vector<size_t>& lost = sweep.mLost;
  std::vector<size_t>& containing = sweep.mContaining;

  for(size_t i = 0; i < count; ++i) {
    gained.clear();
    lost.clear();
    containing.clear();

    auto findX = findLowerBoundaryFirstEqual(sweep.mX, prevBoundaryMinX[i]);
    auto findY = findLowerBoundaryFirstEqual(sweep.mY, prevBoundaryMinY[i]);

    //If new min is lower, swap down and log all gained elements along the way
    moveBoundary(findX, sweep.mX.begin(), sweep.mX.end(), keys[i], newBoundaryMinX[i], gained, lost, &SweepElement::isEnd, &SweepElement::isEnd);
    auto endX = walkToEnd(findX, containing);
    moveBoundary(endX, sweep.mX.begin(), sweep.mX.end(), keys[i], newBoundaryMaxX[i], lost, gained, &SweepElement::isStart, &SweepElement::isStart);

    moveBoundary(findY, sweep.mY.begin(), sweep.mY.end(), keys[i], newBoundaryMinY[i], gained, lost, &SweepElement::isEnd, &SweepElement::isEnd);
    auto endY = walkToEnd(findY, containing);
    moveBoundary(endY, sweep.mY.begin(), sweep.mY.end(), keys[i], newBoundaryMaxY[i], lost, gained, &SweepElement::isStart, &SweepElement::isStart);

    //If either axis was lost, the collision pair is lost
    std::sort(lost.begin(), lost.end());
    lost.erase(std::unique(lost.begin(), lost.end()), lost.end());
    for(const size_t& loss : lost) {
      removedPairs.push_back({ keys[i], loss });
    }

    //If one axis was gained it's only a new pair if the other axis was already overlapping
    //If two axes were gained it's a new pair
    //Sort results to put duplicates next to each-other
    std::sort(gained.begin(), gained.end());
    bool containingSorted = false;
    for(size_t j = 0; j < gained.size();) {
      //Both axes were gained, it's a new pair
      if(j + 1 < gained.size() && gained[j] == gained[j + 1]) {
        newPairs.push_back({ keys[j], gained[j] });
        //Skip forward past both in the pair
        j += 2;
      }
      else {
        //Only one axis was gained, need to see if the other axis already contained this
        if(!containingSorted) {
          std::sort(containing.begin(), containing.end());
          containingSorted = true;
        }
        if(auto isContained = std::lower_bound(containing.begin(), containing.end(), gained[j]); isContained != containing.end() && *isContained == gained[j]) {
          newPairs.push_back({ keys[j], gained[j] });
        }
        ++j;
      }
    }
  }
}
