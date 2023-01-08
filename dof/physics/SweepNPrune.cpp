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
    const size_t toFind = it->getValue();
    ++it;
    while(true) {
      if(it->getValue() != toFind) {
        if(it->isStart()) {
          collect.push_back(it->getValue());
        }
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
    const float firstBoundary = boundary != end ? boundary->mBoundary : 0.0f;
    if(boundary != end) {
      boundary->mBoundary = newBoundary;
    }

    //If new min is lower, swap down and log all gained elements along the way
    if(boundary == end || firstBoundary > newBoundary) {
      if(boundary != begin) {
        --boundary;
      }
      //A bit weirder in the begin case becase the begin needs to be evaluated but not passed
      while(boundary->mBoundary >= newBoundary) {
        if(boundary->getValue() != key && (*boundary.*shouldCollectLower)()) {
          collectLower.push_back(boundary->getValue());
        }
        if(boundary->getValue() != key) {
          std::swap(*boundary, *(boundary + 1));
        }

        if(boundary == begin) {
          break;
        }
        --boundary;
      }
    }
    else {
      //If new min is higher, swap up and log all lost elements along the way
      if(boundary != end) {
        ++boundary;
      }
      while(boundary != end && boundary->mBoundary <= newBoundary) {
        if(boundary->getValue() != key && (*boundary.*shouldCollectHigher)()) {
          collectHigher.push_back(boundary->getValue());
        }
        std::swap(*boundary, *(boundary - 1));

        ++boundary;
      }
    }
  }

  std::pair<float, float> moveBoundaryAxis(std::vector<SweepElement>& axis, size_t key, float prevBoundaryMin, float newBoundaryMin, float newBoundaryMax, std::vector<size_t>& containing, std::vector<size_t>& gained, std::vector<size_t>& lost) {
      auto findX = findKey(axis, key, prevBoundaryMin);
      //If new min is lower, swap down and log all gained elements along the way
      auto endX = walkToEnd(findX, containing);
      const float prevXMin = findX->mBoundary;
      const float prevXMax = endX->mBoundary;
      //Ensure that the boundaries are moved in an order that they don't invalidate each-other.
      //Since max-min is always positive it's not necessary to handle a case where the two iterators cross
      if(newBoundaryMin > prevBoundaryMin) {
        moveBoundary(endX, axis.begin(), axis.end(), key, newBoundaryMax, lost, gained, &SweepElement::isStart, &SweepElement::isStart);
        moveBoundary(findX, axis.begin(), axis.end(), key, newBoundaryMin, gained, lost, &SweepElement::isEnd, &SweepElement::isEnd);
      }
      else {
        moveBoundary(findX, axis.begin(), axis.end(), key, newBoundaryMin, gained, lost, &SweepElement::isEnd, &SweepElement::isEnd);
        moveBoundary(endX, axis.begin(), axis.end(), key, newBoundaryMax, lost, gained, &SweepElement::isStart, &SweepElement::isStart);
      }
      return std::make_pair(prevXMin, prevXMax);
  }

  void _addBoundariesOverlappingEnd(SweepIt begin, SweepIt end, std::vector<size_t>& results) {
    //Start searches here to ignore anything that was already in result container not added by this function
    const size_t startIndex = size_t(std::distance(results.begin(), results.end()));
    while(begin != end) {
      //Traverse the range and add elements when they begin and remove them when they end
      //The end result of the traversal will be all elements that haven't ended yet, meaning
      //they overlap with end
      if(begin->isStart()) {
        results.push_back(begin->getValue());
      }
      else {
        const size_t value = begin->getValue();
        if(auto it = std::find(results.begin() + startIndex, results.end(), value); it != results.end()) {
          *it = results.back();
          results.pop_back();
        }
      }
      ++begin;
    }
  }

  bool _isOverlapping(float aMin, float aMax, float bMin, float bMax) {
    if(aMin > bMin) {
      return _isOverlapping(bMin, bMax, aMin, aMax);
    }
    return aMax >= bMin;
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

    auto endX = walkToEnd(findX, sweep.mLost);
    _addBoundariesOverlappingEnd(sweep.mX.begin(), findX, sweep.mLost);

    auto endY = walkToEnd(findY, sweep.mLost);
    _addBoundariesOverlappingEnd(sweep.mY.begin(), findY, sweep.mLost);

    assert(endX != sweep.mX.end());
    assert(endY != sweep.mY.end());
    //Would be a bit more efficient to manually shift the elements down to avoid double-shifting elements after endX
    //For now the simplicity is preferred
    sweep.mX.erase(endX);
    sweep.mX.erase(findX);
    sweep.mY.erase(endY);
    sweep.mY.erase(findY);

    std::sort(sweep.mLost.begin(), sweep.mLost.end());
    sweep.mLost.erase(std::unique(sweep.mLost.begin(), sweep.mLost.end()), sweep.mLost.end());
    for(const size_t& loss : sweep.mLost) {
      lostPairs.push_back({ keys[i], loss });
    }

    if(auto it = sweep.mKeyToBoundaries.find(keys[i]); it != sweep.mKeyToBoundaries.end()) {
      sweep.mKeyToBoundaries.erase(it);
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
    //These are pretty inefficient, ideally it would be possible to know about these overlaps without
    //having to start at the beginning of the array. Either way insertion is not as common as reinsert
    //so performance isn't critical here
    _addBoundariesOverlappingEnd(sweep.mX.begin(), newBegin, sweep.mGained);
    auto next = newBegin + 1;
    while(next != sweep.mX.end() && next->mBoundary <= newBoundaryMaxX[i]) {
      if(next->isStart()) {
        sweep.mGained.push_back(next->getValue());
      }
      ++next;
    }
    sweep.mX.insert(next, SweepElement::createEnd(newBoundaryMaxX[i], keys[i]));

    newBegin = sweep.mY.insert(findY, SweepElement::createBegin(newBoundaryMinY[i], keys[i]));
    _addBoundariesOverlappingEnd(sweep.mY.begin(), newBegin, sweep.mGained);
    next = newBegin + 1;
    while(next != sweep.mY.end() && next->mBoundary <= newBoundaryMaxY[i]) {
      if(next->isStart()) {
        sweep.mGained.push_back(next->getValue());
      }
      ++next;
    }
    sweep.mY.insert(next, SweepElement::createEnd(newBoundaryMaxY[i], keys[i]));

    //Duplicates indicate they were added on both axes, meaning they are collision pairs
    std::sort(sweep.mGained.begin(), sweep.mGained.end());
    for(size_t j = 0; j < sweep.mGained.size();) {
      if(j + 1 < sweep.mGained.size() && sweep.mGained[j] == sweep.mGained[j + 1]) {
        newPairs.push_back({ keys[i], sweep.mGained[j] });
        j += 2;
      }
      else {
        ++j;
      }
    }

    Sweep2D::Bounds& bounds = sweep.mKeyToBoundaries[keys[i]];
    bounds.mMinX = newBoundaryMinX[i];
    bounds.mMaxX = newBoundaryMaxX[i];
    bounds.mMinY = newBoundaryMinY[i];
    bounds.mMaxY = newBoundaryMaxY[i];
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

    const std::pair<float, float> prevX = moveBoundaryAxis(sweep.mX, keys[i], prevBoundaryMinX[i], newBoundaryMinX[i], newBoundaryMaxX[i], containing, gained, lost);
    const float prevXMin = prevX.first;
    const float prevXMax = prevX.second;

    const std::pair<float, float> prevY = moveBoundaryAxis(sweep.mY, keys[i], prevBoundaryMinY[i], newBoundaryMinY[i], newBoundaryMaxY[i], containing, gained, lost);
    const float prevYMin = prevY.first;
    const float prevYMax = prevY.second;

    //If either axis was lost, the collision pair is lost if that was also previously overlapping on the other axis
    std::sort(lost.begin(), lost.end());
    for(size_t j = 0; j < lost.size();) {
      const size_t& loss = lost[j];
      if(j + 1 < lost.size() && lost[j + 1] == loss) {
        removedPairs.push_back({ keys[i], loss });
        j += 2;
        continue;
      }
      //Check to see if this was previously overlapping
      if(auto boundsIt = sweep.mKeyToBoundaries.find(loss); boundsIt != sweep.mKeyToBoundaries.end()) {
        const Sweep2D::Bounds& bounds = boundsIt->second;
        if(_isOverlapping(bounds.mMinX, bounds.mMaxX, prevXMin, prevXMax) &&
          _isOverlapping(bounds.mMinY, bounds.mMaxY, prevYMin, prevYMax)) {
          removedPairs.push_back({ keys[i], loss });
        }
      }
      ++j;
    }

    //If one axis was gained it's only a new pair if the other axis was already overlapping
    //If two axes were gained it's a new pair
    //Sort results to put duplicates next to each-other
    std::sort(gained.begin(), gained.end());
    bool containingSorted = false;
    for(size_t j = 0; j < gained.size();) {
      //Both axes were gained, it's a new pair
      if(j + 1 < gained.size() && gained[j] == gained[j + 1]) {
        newPairs.push_back({ keys[i], gained[j] });
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
          newPairs.push_back({ keys[i], gained[j] });
        }
        //If it wasn't in th econtained list, try the boundary map and check for overlap
        else if(auto boundsIt = sweep.mKeyToBoundaries.find(gained[j]); boundsIt != sweep.mKeyToBoundaries.end()) {
          const Sweep2D::Bounds& bounds = boundsIt->second;
          if(_isOverlapping(bounds.mMinX, bounds.mMaxX, newBoundaryMinX[i], newBoundaryMaxX[i]) &&
            _isOverlapping(bounds.mMinY, bounds.mMaxY, newBoundaryMinY[i], newBoundaryMaxY[i])) {
            newPairs.push_back({ keys[i], gained[j] });
          }
        }
        ++j;
      }
    }

    Sweep2D::Bounds& bounds = sweep.mKeyToBoundaries[keys[i]];
    bounds.mMinX = newBoundaryMinX[i];
    bounds.mMaxX = newBoundaryMaxX[i];
    bounds.mMinY = newBoundaryMinY[i];
    bounds.mMaxY = newBoundaryMaxY[i];
  }
}
