#include "Precompile.h"
#include "SweepNPrune.h"

#include <cassert>

#include "glm/common.hpp"
#include "Scheduler.h"
#include "Profile.h"
#include "AppBuilder.h"
#include "SweepNPruneBroadphase.h"

//#define BROADPHASE_DEBUG
#ifdef BROADPHASE_DEBUG
#define BROADPHASE_ASSERT(condition) assert(condition)
#else
#define BROADPHASE_ASSERT(condition)
#endif

namespace Broadphase {
  namespace Debug {
    bool isValidSweepAxis(const SweepAxis& axis) {
      std::unordered_map<BroadphaseKey, uint8_t> traversed;
      traversed.reserve(axis.elements.size() / 2);
      for(const SweepElement& e : axis.elements) {
        const BroadphaseKey key{ e.getValue() };
        if(e.isStart()) {
          //Elements should not be duplicated and start should always appear before end
          if(!traversed.insert(std::make_pair(key, uint8_t{})).second) {
            return false;
          }
        }
        else {
          auto it = traversed.find(key);
          //End should always exist if there is a start, and come afterwards
          if(it == traversed.end()) {
            return false;
          }
          //There should only be one end
          if(it->second++) {
            return false;
          }
        }
      }
      //All elements should have found a single paired end
      return std::all_of(traversed.begin(), traversed.end(), [](const auto& pair) { return pair.second == 1; });
    }
  };

  struct ElementBounds {
    bool operator<(const ElementBounds& rhs) const {
      //It's important to ensure start and end don't pass each-other but the worst they can do is match
      //In which case the sort will not swap them past each other as they stop when < is false
      return value < rhs.value;
    }
    SweepElement element;
    const std::pair<float, float>& bounds;
    const float value{};
  };

  ElementBounds unwrapElement(const SweepElement& element, const std::pair<float, float>* bounds) {
    const size_t v = element.getValue();
    return {
      element,
      bounds[v],
      element.isStart() ? bounds[v].first : bounds[v].second
    };
  }

  bool isOverlapping(const std::pair<float, float>& l, const std::pair<float, float>& r) {
    if(l.second < r.first) {
      //L entirely less than R
      return false;
    }
    if(l.first > r.second) {
      //L entirely greater than R
      return false;
    }
    if(l.first == ObjectDB::REMOVED && r.first == ObjectDB::REMOVED) {
      //They were removed, treat this as not overlapping so the pair tracking is removed
      return false;
    }
    return true;
  }

  bool isOverlapping(const ElementBounds& l, const ElementBounds& r) {
    return isOverlapping(l.bounds, r.bounds);
  }

  struct SwapLogArgs {
    const ObjectDB::BoundsMinMax* primaryAxis{};
    const ObjectDB::BoundsMinMax* secondaryAxis{};
    const PairTracker* tracked{};
    SwapLog* log{};
  };

  bool isOverlapping(const ElementBounds& lPrimary, SweepElement r, const SwapLogArgs& args) {
    return isOverlapping(lPrimary.bounds, args.primaryAxis[r.getValue()]) &&
      isOverlapping(args.secondaryAxis[lPrimary.element.getValue()], args.secondaryAxis[r.getValue()]);
  }

  //Left is a start element moving back over right
  void logSwapBackStart(const ElementBounds& left, SweepElement right, SwapLogArgs& args) {
    if(right.isStart()) {
      //Left start passed over right start meaning left end might still be within it, nothing to do yet
    }
    else {
      //Left start passed over right end, starting to overlap with right edge
      //Only care about this if it would be a new pair (not tracked) and they are newly overlapping on both axes
      const SweepCollisionPair pair{ left.element.getValue(), right.getValue() };
      if(!args.tracked->trackedPairs.count(pair)) {
        if(isOverlapping(left, right, args)) {
          args.log->gains.emplace_back(pair);
        }
      }
    }
  }

  //Left is an end element moving back over right
  void logSwapBackEnd(const ElementBounds& left, SweepElement right, SwapLogArgs& args) {
    if(right.isEnd()) {
      //Left end moved over right end meaning left end might still be within right start, nothing to do yet
    }
    else {
      //Left end swapped over right begin, overlap may have ended
      //Only care about this if they were colliding (tracked pair) and are no longer overlapping on at least one axis
      const SweepCollisionPair pair{ left.element.getValue(), right.getValue() };
      if(args.tracked->trackedPairs.count(pair)) {
        const auto r = unwrapElement(right, args.primaryAxis);
        //Only check the primary axis here, if it's not overlapping on the other axis it'll get logged when sorting that axis
        if(!isOverlapping(left, r)) {
          args.log->losses.emplace_back(pair);
        }
      }
    }
  }

  void insertionSort(SweepElement* first,
    SweepElement* last,
    SwapLogArgs& args
  ) {
    if(first == last) {
      return;
    }
    const ObjectDB::BoundsMinMax* bounds = args.primaryAxis;

    //Order next element
    for(SweepElement* mid = first; ++mid != last;) {
      const SweepElement value = *mid;
      const ElementBounds valueBounds = unwrapElement(value, bounds);
      const ElementBounds firstBounds = unwrapElement(*first, bounds);
      auto logSwap = value.isStart() ? &logSwapBackStart : &logSwapBackEnd;

      //Found new earliest element, move to front
      if(valueBounds < firstBounds) {
        SweepElement* current = mid;
        //Swap this element with all between first and mid, shifting right to make space
        while(current != first) {
          logSwap(valueBounds, *current, args);
          *current = *(current - 1);
          --current;
        }
        //Insert element at beginning
        *current = value;
      }
      else {
        //Look for insertion point after first going back from mid, shifting them right to make space to insert
        SweepElement* current = mid - 1;
        while(current != first) {
          const ElementBounds c = unwrapElement(*current, bounds);
          //Keep searching while the value to insert is lower. If it's equal or greater, stop
          if(valueBounds < c) {
            logSwap(valueBounds, *current, args);
            *(current + 1) = *current;
            --current;
          }
          else {
            break;
          }
        }
        //Insert element at new insertion point
        *(current + 1) = value;
      }
    }
  }

  BroadphaseKey getOrCreateKey(std::vector<BroadphaseKey>& freeList, BroadphaseKey& newKey) {
    if(!freeList.empty()) {
      const BroadphaseKey result = freeList.back();
      freeList.pop_back();
      return result;
    }
    return { newKey.value++ };
  }

  void insertRange(ObjectDB& db,
    const UserKey* userKeys,
    BroadphaseKey* outKeys,
    size_t count
  ) {
    BroadphaseKey newKey = { db.userKey.size() };
    if(count > db.freeList.size()) {
      const size_t slotsNeeded = count - db.freeList.size();
      for(size_t s = 0; s < ObjectDB::S; ++s) {
        db.bounds[s].resize(db.bounds[s].size() + slotsNeeded);
      }
      db.userKey.resize(db.userKey.size() + slotsNeeded);
    }
    for(size_t i = 0; i < count; ++i) {
      const BroadphaseKey k = getOrCreateKey(db.freeList, newKey);

      outKeys[i] = { k };
      db.userKey[k.value] = userKeys[i];

      for(size_t d = 0; d < db.bounds.size(); ++d) {
        db.bounds[d][k.value] = { ObjectDB::NEW, ObjectDB::NEW };
      }
    }
  }

  void eraseRange(ObjectDB& db,
    const BroadphaseKey* keys,
    size_t count
  ) {
    //Put this key in the pending removal list, not yet the free list so it doesn't get re-used
    //This allows the bounds to be written as out of bounds and then for the next recompute to process
    //the removals rather than having to do a synchronous search here
    const size_t base = db.pendingRemoval.size();
    db.pendingRemoval.resize(db.pendingRemoval.size() + count);

    for(size_t i = 0; i < count; ++i) {
      const BroadphaseKey& k = keys[i];
      db.pendingRemoval[base + i] = k;

      //Write the new bounds so that the key is out of the way
      for(size_t d = 0; d < ObjectDB::S; ++d) {
        db.bounds[d][k.value] = { ObjectDB::REMOVED, ObjectDB::REMOVED };
      }
      //User key needs to be left as-is so it can show up for removal of pairs
    }
  }

  void updateBoundaries(ObjectDB& db,
    const float* minX,
    const float* maxX,
    const float* minY,
    const float* maxY,
    const BroadphaseKey* keys,
    size_t count
  ) {
    //Size of both axes are the same
    const size_t size = db.bounds[0].size();
    for(size_t i = 0; i < count; ++i) {
      //Bounds check mainly for default constructed EMPTY_KEY used for elements that haven't been inserted into broadphase yet
      if(const size_t k = keys[i].value; k < size) {
        db.bounds[0][k] = { minX[i], maxX[i] };
        db.bounds[1][k] = { minY[i], maxY[i] };
      }
    }
  }

  void logChangedPairs(const ObjectDB& db, PairTracker& pairs, const ConstSwapLog& changedPairs, SwapLog& output) {
    for(const SweepCollisionPair& g : changedPairs.gains) {
      if(pairs.trackedPairs.insert(g).second) {
        //Up until now the pairs have been holding BroadphaseKeys. For the event, look up the corresponding UserKey
        output.gains.emplace_back(db.userKey[g.a], db.userKey[g.b]);
      }
    }
    for(const SweepCollisionPair& l : changedPairs.losses) {
      if(auto it = pairs.trackedPairs.find(l); it != pairs.trackedPairs.end()) {
        pairs.trackedPairs.erase(it);
        output.losses.emplace_back(db.userKey[l.a], db.userKey[l.b]);
      }
    }
  }

  //Log events for pairs that will be removed as a result of pending removals, but don't remove yet
  void logPendingRemovals(const ObjectDB& db, SwapLog& log, const PairTracker& pairs) {
    //All removal events should have been logged, now insert removals into free list and trim them off the end of the axes
    //Removals would always be at the end because REMOVED is float max and it was just sorted above
    if(size_t toRemove = db.pendingRemoval.size()) {
      //Log an event for all removal pairs. This is because all removal bounds are equal so their events might be missed if both were removed
      //resolveCandidates will remove redundancies
      for(size_t i = 0; i < db.pendingRemoval.size(); ++i) {
        for(size_t j = i + 1; j < db.pendingRemoval.size(); ++j) {
          const SweepCollisionPair pair{ db.pendingRemoval[i].value, db.pendingRemoval[j].value };
          if(pairs.trackedPairs.count(pair)) {
            log.losses.emplace_back(pair);
          }
        }
      }
    }
  }

  //Remove elements pending deletion. This is after the events for them have already been logged and no cells are referencing them anymore
  void processPendingRemovals(ObjectDB& db) {
    //All removal events should have been logged, now insert removals into free list and trim them off the end of the axes
    //Removals would always be at the end because REMOVED is float max and it was just sorted above
    if(size_t toRemove = db.pendingRemoval.size()) {
      //Log an event for all removal pairs. This is because all removal bounds are equal so their events might be missed if both were removed
      //resolveCandidates will remove redundancies
      for(size_t i = 0; i < db.pendingRemoval.size(); ++i) {
        db.userKey[db.pendingRemoval[i].value] = ObjectDB::EMPTY;
      }
      db.freeList.insert(db.freeList.end(), db.pendingRemoval.begin(), db.pendingRemoval.end());
      db.pendingRemoval.clear();
    }
  }

  namespace SweepNPrune {
    void insertRange(Sweep2D& sweep,
      const BroadphaseKey* keys,
      size_t count
    ) {
      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[0]));
      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[1]));

      //Axis doesn't use the free list, always add to end
      size_t newAxisIndex = sweep.axis[0].elements.size();
      for(size_t s = 0; s < Sweep2D::S; ++s) {
        sweep.axis[s].elements.resize(sweep.axis[s].elements.size() + (count * 2));
      }

      for(size_t i = 0; i < count; ++i) {
        const BroadphaseKey& k = keys[i];
        for(size_t d = 0; d < sweep.axis.size(); ++d) {
          //These are put at the end now and will be addressed in the next recomputePairs
          sweep.axis[d].elements[newAxisIndex] = SweepElement::createBegin(k);
          sweep.axis[d].elements[newAxisIndex + 1] = SweepElement::createEnd(k);
        }
        newAxisIndex += 2;
      }

      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[0]));
      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[1]));
    }

    void tryInsertRange(Sweep2D& sweep,
      const BroadphaseKey* keys,
      size_t count
    ) {
      for(size_t i = 0; i < count; ++i) {
        if(sweep.containedKeys.insert(keys[i]).second) {
          insertRange(sweep, keys + i, 1);
        }
      }
    }

    void recomputeCandidates(Sweep2D& sweep, const ObjectDB& db, const PairTracker& pairs, SwapLog& log) {
      PROFILE_SCOPE("physics", "computeCandidates");
      SwapLogArgs args{
        nullptr,
        nullptr,
        &pairs,
        &log
      };
      for(size_t i = 0; i < Sweep2D::S; ++i) {
        BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[i]));
        args.primaryAxis = db.bounds[i].data();
        args.secondaryAxis = db.bounds[(i + 1) % Sweep2D::S].data();
        insertionSort(
          sweep.axis[i].elements.data(),
          sweep.axis[i].elements.data() + sweep.axis[i].elements.size(),
          args
        );
        BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[i]));
      }
    }

    struct SweepElementQuery {
      float position{};
      BroadphaseKey key{};
      bool isStart{};
    };

    struct SweepElementCompare {
      bool operator()(const SweepElement& l, const SweepElementQuery& r) const {
        return getBounds(l) < r.position;
      }

      float getBounds(const SweepElement& e) const {
        const auto& range = db.bounds[axis][e.getValue()];
        return e.isStart() ? range.first : range.second;
      }

      const ObjectDB& db;
      size_t axis{};
    };

    //Remove elements that are no longer within the boundaries of this Sweep2D
    void trimBoundaries(const ObjectDB& db, Sweep2D& sweep) {
      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[0]));
      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[1]));
      std::vector<BroadphaseKey>& toRemove = sweep.temp;

      const size_t count = sweep.axis[0].elements.size();
      if(!count) {
        return;
      }
      for(size_t axis = 0; axis < sweep.axis.size(); ++axis) {
        //Find objects off the min side of the axes
        const float minBound = sweep.axis[axis].min;
        for(size_t i = 0; i < count; ++i) {
          const SweepElement& e = sweep.axis[axis].elements[i];
          const auto& bounds = db.bounds[axis][e.getValue()];
          if(bounds.first >= minBound) {
            break;
          }
          if(e.isEnd()) {
            toRemove.push_back(BroadphaseKey{ e.getValue() });
          }
        }
        //Find objects off the max side of the axes
        const float maxBound = sweep.axis[axis].max;
        for(size_t i = 0; i < count; ++i) {
          const size_t ri = count - i - 1;
          const SweepElement& e = sweep.axis[axis].elements[ri];
          const auto& bounds = db.bounds[axis][e.getValue()];
          if(bounds.second <= maxBound) {
            break;
          }
          if(e.isStart()) {
            toRemove.push_back(BroadphaseKey{ e.getValue() });
          }
        }
      }

      //Remove all the elements. This is rough because their position was only known on one axis
      //so a binary search is needed to find it on the other
      for(const BroadphaseKey& remove : toRemove) {
        auto it = sweep.containedKeys.find(remove);
        //Already removed, skip this one
        if(it == sweep.containedKeys.end()) {
          continue;
        }
        sweep.containedKeys.erase(it);

        for(size_t axis = 0; axis < sweep.axis.size(); ++axis) {
          auto& axisElements = sweep.axis[axis].elements;
          SweepElementQuery query{
            db.bounds[axis][remove.value].first,
            remove,
            true
          };
          auto beginIt = std::lower_bound(axisElements.begin(), axisElements.end(), query, SweepElementCompare{ db, axis });
          //Should always be found
          if(beginIt != axisElements.end()) {
            //Position has been found with lower bound but it still might be a different key
            //Scan along and find entries to remove by shifting everything down into the gaps
            auto nextToMove = beginIt;
            auto toFill = beginIt;
            while(nextToMove != axisElements.end()) {
              if(nextToMove->getValue() == remove.value) {
                //Look ahead one extra entry for this one to fill this end entry to remove
                nextToMove++;
                continue;
              }
              else if(toFill != nextToMove) {
                *toFill = *nextToMove;
              }
              ++toFill;
              ++nextToMove;
            }
            //Should always be two removed elements and never less than axisElements
            const size_t removedCount = nextToMove - toFill;
            if(removedCount <= axisElements.size()) {
              axisElements.resize(axisElements.size() - removedCount);
            }
          }
        }
      }

      toRemove.clear();
    }

    void recomputePairs(Sweep2D& sweep, const ObjectDB& db, const PairTracker& pairs, SwapLog& log) {
      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[0]));
      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[1]));

      recomputeCandidates(sweep, db, pairs, log);
      trimBoundaries(db, sweep);

      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[0]));
      BROADPHASE_ASSERT(Debug::isValidSweepAxis(sweep.axis[1]));
    }
  };

  namespace SweepGrid {
    void setIf(float& value, bool condition, float toSet) {
      if(condition) {
        value = toSet;
      }
    }
    void init(Grid& grid) {
      constexpr float posInfinity = std::numeric_limits<float>::max();
      constexpr float negInfinity = std::numeric_limits<float>::lowest();
      grid.cells.resize(grid.definition.cellsX*grid.definition.cellsY);
      for(size_t x = 0; x < grid.definition.cellsX; ++x) {
        for(size_t y = 0; y < grid.definition.cellsY; ++y) {
          glm::vec2 min = grid.definition.bottomLeft;
          min.x += static_cast<float>(x) * grid.definition.cellSize.x;
          min.y += static_cast<float>(y) * grid.definition.cellSize.y;
          glm::vec2 max = min + grid.definition.cellSize;

          //Extend to infinity on boundaries
          min.x = x == 0 ? negInfinity : min.x;
          min.y = y == 0 ? negInfinity : min.y;
          max.x = x + 1 == grid.definition.cellsX ? posInfinity : max.x;
          max.y = y + 1 == grid.definition.cellsY ? posInfinity : max.y;

          Sweep2D& cell = grid.cells[x + y*grid.definition.cellsX];
          for(size_t i = 0; i < cell.axis.size(); ++i) {
            cell.axis[i].min = min[i];
            cell.axis[i].max = max[i];
          }
        }
      }
    }

    void insertRange(Grid& grid,
      const UserKey* userKeys,
      BroadphaseKey* outKeys,
      size_t count) {
      //Assign the keys a slot but don't map them to any internal cells yet since bounds aren't known
      //This will happen during updateBoundaries
      Broadphase::insertRange(grid.objects, userKeys, outKeys, count);
    }

    void eraseRange(Grid& grid,
      const BroadphaseKey* keys,
      size_t count) {
      Broadphase::eraseRange(grid.objects, keys, count);
      //UserKey gets removed at the end of the next update upon processing pending removals
    }

    struct Bounds {
      glm::vec2 min{};
      glm::vec2 max{};
    };

    //Call fn with index for all cells that the bounds overlap with.
    template<class FN>
    void foreachCell(const GridDefinition& definition, const Bounds& bounds, FN&& fn) {
      //Translate to local space such that each cell is of size 1 and the bottom left cell is at the origin
      const glm::vec2 invScale{ 1.0f/definition.cellSize.x, 1.0f/definition.cellSize.y };
      const glm::vec2 boundaryMin = glm::vec2(0, 0);
      const glm::vec2 boundaryMax = glm::vec2(definition.cellsX - 1, definition.cellsY - 1);
      const glm::vec2 localMin = glm::clamp((bounds.min - definition.bottomLeft)*invScale, boundaryMin, boundaryMax);
      const glm::vec2 localMax = glm::clamp((bounds.max - definition.bottomLeft)*invScale, boundaryMin, boundaryMax);
      //Now that they are in local space truncation to int can be used to find all the desired indices
      for(int y = static_cast<int>(localMin.y); y <= static_cast<int>(localMax.y); ++y) {
        for(int x = static_cast<int>(localMin.x); x <= static_cast<int>(localMax.x); ++x) {
          fn(static_cast<size_t>(x + y*definition.cellsX));
        }
      }
    }

    void updateBoundaries(Grid& grid,
      const float* minX,
      const float* maxX,
      const float* minY,
      const float* maxY,
      const BroadphaseKey* keys,
      size_t count) {
      for(size_t i = 0; i < count; ++i) {
        if(keys[i] == EMPTY_KEY) {
          continue;
        }
        //Update the bounds information all cells are referring to for this single key
        Broadphase::updateBoundaries(grid.objects,
          minX + i,
          maxX + i,
          minY + i,
          maxY + i,
          keys + i,
          1);

        const Bounds newBounds{ glm::vec2{ minX[i], minY[i] }, glm::vec2{ maxX[i], maxY[i] } };
        //TODO: skip reinsertion check if new bounds aren't outside of cells from last time
        //Try to insert into each cell this overlaps with, skipping if they're already there
        foreachCell(grid.definition, newBounds, [&](size_t cellIndex) {
          if(cellIndex < grid.cells.size()) {
            SweepNPrune::tryInsertRange(grid.cells[cellIndex], keys + i, 1);
          }
        });
        //Removal happens during recomputePairs when cells realize elements are outside of their intended boundaries
      }
    }

    void recomputePairs(IAppBuilder& builder) {
      struct TaskData {
        std::vector<Broadphase::SweepCollisionPair> gains, losses;
      };
      struct Tasks {
        std::vector<TaskData> tasks;
      };

      auto configure = builder.createTask();
      auto process = builder.createTask();
      auto combine = builder.createTask();
      configure.setName("configure broadphase tasks");
      process.setName("recompute broadphase pairs");
      combine.setName("combine final broadphse results");
      auto data = std::make_shared<Tasks>();

      {
        auto processConfig = process.getConfig();
        const auto& grid = *configure.query<const SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
        configure.setCallback([&grid, processConfig, data](AppTaskArgs&) mutable {
          AppTaskSize s;
          s.batchSize = 1;
          s.workItemCount = grid.cells.size();
          processConfig->setSize(s);
          data->tasks.resize(grid.cells.size());
        });
        builder.submitTask(std::move(configure));
      }
      {
        auto& grid = *process.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
        process.setCallback([&grid, data](AppTaskArgs& args) mutable {
          for(size_t i = args.begin; i < args.end; ++i) {
            SwapLog log {
              data->tasks[i].gains,
              data->tasks[i].losses
            };
            log.gains.clear();
            log.losses.clear();
            SweepNPrune::recomputePairs(grid.cells[i], grid.objects, grid.pairs, log);
          }
        });
        builder.submitTask(std::move(process));
      }
      {
        //Dependency on grid so the previous finishes first
        auto grid = combine.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
        auto& finalResults = *combine.query<SharedRow<SweepNPruneBroadphase::PairChanges>>().tryGetSingletonElement();

        combine.setCallback([&finalResults, data, grid](AppTaskArgs&) mutable {
          finalResults.mGained.clear();
          finalResults.mLost.clear();

          if(data->tasks.size()) {
            //Doesn't matter which log these show up in, arbitrarily choose 0. They are processed like any other loss below
            SwapLog log{ data->tasks[0].gains, data->tasks[0].losses };
            Broadphase::logPendingRemovals(grid->objects, log, grid->pairs);
          }

          //All of the tasks have logged a series of gains and losses compared to what is currently in PairTracker
          //Due to overlapping cells some of the pairs may have duplicate information
          //Tracked pairs can be updated now and used to discard the redundant events based on if the event would change
          //what is tracked
          SwapLog results{ finalResults.mGained, finalResults.mLost };
          for(size_t i = 0; i < data->tasks.size(); ++i) {
            const TaskData& t = data->tasks[i];
            const ConstSwapLog taskLog{ t.gains, t.losses };
            Broadphase::logChangedPairs(grid->objects, grid->pairs, taskLog, results);
          }

          //Now that everything is done, actually remove pending elements
          Broadphase::processPendingRemovals(grid->objects);
        });
        builder.submitTask(std::move(combine));
      }
    }
  }
}