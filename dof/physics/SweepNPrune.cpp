#include "Precompile.h"
#include "SweepNPrune.h"

#include <cassert>

#include "glm/common.hpp"
#include "Scheduler.h"
#include "Profile.h"
#include "AppBuilder.h"
#include "SweepNPruneBroadphase.h"

namespace Broadphase {
  struct ElementBounds {
    bool operator<(const ElementBounds& rhs) const {
      //Sort by bounds unless they match in which case preserve key order
      //Main poitn is this prevents swapping a start and end with itself
      return value == rhs.value ? element.value < rhs.element.value : value < rhs.value;
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
    if(l.first == Sweep2D::REMOVED && r.first == Sweep2D::REMOVED) {
      //They were removed, treat this as not overlapping so the pair tracking is removed
      return false;
    }
    return true;
  }

  bool isOverlapping(const ElementBounds& l, const ElementBounds& r) {
    return isOverlapping(l.bounds, r.bounds);
  }

  //Left is a start element moving back over right
  void logSwapBackStart(const ElementBounds& left, SweepElement right, const std::pair<float, float>* bounds, CollisionCandidates& candidates) {
    if(right.isStart()) {
      //Left start passed over right start meaning left end might still be within it, nothing to do yet
    }
    else {
      //Left start passed over right end, starting to overlap with right edge
      const auto r = unwrapElement(right, bounds);
      if(isOverlapping(left, r)) {
        candidates.pairs.emplace_back(left.element.getValue(), r.element.getValue());
      }
    }
  }

  //Left is an end element moving back over right
  void logSwapBackEnd(const ElementBounds& left, SweepElement right, const std::pair<float, float>* bounds, CollisionCandidates& candidates) {
    if(right.isEnd()) {
      //Left end moved over right end meaning left end might still be within right start, nothing to do yet
    }
    else {
      //Left end swapped over right begin, overlap may have ended
      const auto r = unwrapElement(right, bounds);
      if(!isOverlapping(left, r)) {
        candidates.pairs.emplace_back(left.element.getValue(), r.element.getValue());
      }
    }
  }

  void insertionSort(SweepElement* first,
    SweepElement* last,
    const std::pair<float, float>* bounds,
    CollisionCandidates& candidates) {
    if(first == last) {
      return;
    }
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
          logSwap(valueBounds, *current, bounds, candidates);
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
          if(c < valueBounds) {
            break;
          }

          logSwap(valueBounds, *current, bounds, candidates);
          *(current + 1) = *current;
          --current;
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

  void processPendingRemovals(ObjectDB& db, CollisionCandidates& losses) {
    //All removal events should have been logged, now insert removals into free list and trim them off the end of the axes
    //Removals would always be at the end because REMOVED is float max and it was just sorted above
    if(size_t toRemove = db.pendingRemoval.size()) {
      //Log an event for all removal pairs. This is because all removal bounds are equal so their events might be missed if both were removed
      //resolveCandidates will remove redundancies
      for(size_t i = 0; i < db.pendingRemoval.size(); ++i) {
        for(size_t j = i + 1; j < db.pendingRemoval.size(); ++j) {
          losses.pairs.emplace_back(db.pendingRemoval[i].value, db.pendingRemoval[j].value);
        }
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
    }

    void recomputeCandidates(Sweep2D& sweep, const ObjectDB& db, CollisionCandidates& candidates) {
      PROFILE_SCOPE("physics", "computeCandidates");

      for(size_t i = 0; i < Sweep2D::S; ++i) {
        insertionSort(sweep.axis[i].elements.data(), sweep.axis[i].elements.data() + sweep.axis[i].elements.size(), db.bounds[i].data(), candidates);
      }
    }

    //TODO: if this step is lifted out to the accumulate phase I think it can avoid the need for reference counting
    void resolveCandidates(Sweep2D& sweep, const ObjectDB& db, CollisionCandidates& collisionCandidates, SwapLog log) {
      PROFILE_SCOPE("physics", "resolveCandidates");

      //Remove duplicates. A side benefit of sorted remove is the memory access of bounds is potentially more linear
      std::vector<SweepCollisionPair>& candidates = collisionCandidates.pairs;
      std::sort(candidates.begin(), candidates.end());
      candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
      //Now double check the bounds of the candidates, both to determine if it was a gain and loss and also to confirm in the first place
      for(const SweepCollisionPair& candidate : candidates) {
        bool isColliding = true;
        for(size_t d = 0; d < Sweep2D::S; ++d) {
          //Candidates res-use SweepCollisionPair for convenience but contain broadphase keys, not user keys, so can be used to get bounds here
          if(!isOverlapping(db.bounds[d][candidate.a], db.bounds[d][candidate.b])) {
            isColliding = false;
            break;
          }
        }
        const SweepCollisionPair userPair{ db.userKey[candidate.a], db.userKey[candidate.b] };
        //The collision information is accurate but it can produce redundant events particular for removals for pairs that weren't colliding in the first place
        if(isColliding) {
          if(sweep.trackedPairs.insert(userPair).second) {
            log.gains.emplace_back(userPair);
          }
        }
        else if(auto it = sweep.trackedPairs.find(userPair); it != sweep.trackedPairs.end()) {
          sweep.trackedPairs.erase(it);
          log.losses.emplace_back(userPair);
        }
      }
      candidates.clear();
    }

    struct SweepElementQuery {
      float position{};
      BroadphaseKey key{};
      bool isStart{};
    };

    struct SweepElementCompare {
      bool operator()(const SweepElement& l, const SweepElementQuery& r) const {
        const float lv = getBounds(l);
        const float rv = r.position;
        if(lv == rv) {
          if(l.getValue() == r.key.value) {
            return l.isStart() < r.isStart;
          }
          return l.getValue() < r.key.value;
        }
        return lv < rv;
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
      std::vector<BroadphaseKey> toRemove;

      //TODO: this can easily find elements that are outside on one axis, but then how to find them on the other axis?
      const size_t count = sweep.axis[0].elements.size();
      if(!count) {
        return;
      }
      for(size_t axis = 0; axis < sweep.axis.size(); ++axis) {
        //Find objects off the min side of the axes
        for(size_t i = 0; i < count; ++i) {
          const SweepElement& e = sweep.axis[axis].elements[i];
          if(e.isEnd()) {
            if(db.bounds[0][e.getValue()].second < sweep.axis[axis].min) {
              toRemove.push_back(BroadphaseKey{ e.getValue() });
            }
            else {
              break;
            }
          }
        }
        //Find objects off the max side of the axes
        for(size_t i = 0; i < count; ++i) {
          const size_t ri = count - i - 1;
          const SweepElement& e = sweep.axis[axis].elements[ri];
          if(e.isStart()) {
            if(db.bounds[0][e.getValue()].second > sweep.axis[axis].min) {
              toRemove.push_back(BroadphaseKey{ e.getValue() });
            }
            else {
              break;
            }
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
            //Remove this and its accompanying end entry by shifting everything else down
            while(true) {
              auto next = beginIt + 1;
              if(next == axisElements.end()) {
                break;
              }
              if(next->getValue() == remove.value) {
                //Look ahead one extra entry for this one to fill this end entry to remove
                if(auto extra = next + 1; extra != axisElements.end()) {
                  *beginIt = *extra;
                }
              }
              else {
                *beginIt = *next;
              }
              beginIt = next;
            }
          }
        }
      }
    }

    void recomputePairs(Sweep2D& sweep, const ObjectDB& db, CollisionCandidates& candidates, SwapLog& log) {
      recomputeCandidates(sweep, db, candidates);
      resolveCandidates(sweep, db, candidates, log);
      trimBoundaries(db, sweep);
    }
  };

  namespace SweepGrid {
    void init(Grid& grid) {
      grid.cells.resize(grid.definition.cellsX*grid.definition.cellsY);
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
      //TODO: when does userKey get removed?
    }

    struct Bounds {
      glm::vec2 min{};
      glm::vec2 max{};
    };

    //Given a cell key, find the boudns of the cell
    Bounds getCellBounds(const GridDefinition& definition, BroadphaseKey cellKey) {
      const size_t xIndex = (cellKey.value % definition.cellsX);
      const size_t yIndex = (cellKey.value / definition.cellsX);
      const glm::vec2 min {
        definition.bottomLeft.x + static_cast<float>(xIndex)*definition.cellSize.x,
        definition.bottomLeft.y + static_cast<float>(yIndex)*definition.cellSize.y
      };
      return {
        min,
        min + definition.cellSize
      };
    }

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
          minX,
          maxX,
          minY,
          maxY,
          keys + i,
          1);

        const Bounds newBounds{ glm::vec2{ minX[i], minY[i] }, glm::vec2{ maxX[i], maxY[i] } };
        //TODO: skip reinsertion check if new bounds aren't outside of cells from last time
        //Try to insert into each cell this overlaps with, skipping if they're already there
        foreachCell(grid.definition, newBounds, [&](size_t cellIndex) {
          if(cellIndex < grid.cells.size()) {
            Sweep2D& cell = grid.cells[cellIndex];
            if(cell.containedKeys.insert(keys[i]).second) {
              SweepNPrune::insertRange(cell, keys + i, 1);
            }
          }
        });
        //Removal happens during recomputePairs when cells realize elements are outside of their intended boundaries
      }
    }

    void recomputePairs(IAppBuilder& builder) {
      struct TaskData {
        std::vector<Broadphase::SweepCollisionPair> gains, losses;
        Broadphase::CollisionCandidates candidates;
      };
      struct Tasks {
        struct Tracker {
          enum class Action : uint8_t {
            Add,
            Remove,
            Noop,
            Update
          };

          //Returns true if this may need to change tracked pairs, meaning either addition of a new pair or removal of one
          void increment(int8_t amount) {
            if(!originalRefCount) {
              originalRefCount = currentRefCount;
            }
            currentRefCount += amount;
          }

          Action getAction() const {
            assert(originalRefCount && "Action only makes sense in the context of an original ref count");
            assert(currentRefCount >= 0 && "Counting should always be balanced");
            if(!*originalRefCount) {
              //If it went from zero to positive, new element added
              //It it went from zero to zero then this element should be removed again without being published
              return currentRefCount > 0 ? Action::Add : Action::Noop;
            }
            //Nonzero original, presumably this means positive since counting shouldn't be unbalanced
            //This means hitting zero would be removal
            return !currentRefCount ? Action::Remove : Action::Update;
          }

          std::optional<int8_t> originalRefCount{};
          int8_t currentRefCount{};
        };

        std::vector<TaskData> tasks;
        //Hack to reference count pairs to avoid duplicate events for pairs in multiple cells
        std::unordered_map<Broadphase::SweepCollisionPair, Tracker> trackedPairs;
        std::vector<std::pair<const Broadphase::SweepCollisionPair, Tracker>*> changedTrackers;
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
            data->tasks[i].candidates.pairs.clear();
            SweepNPrune::recomputePairs(grid.cells[i], data->tasks[i].candidates, log);
          }
        });
        builder.submitTask(std::move(process));
      }
      {
        //Dependency on grid so the previous finishes first
        auto grid = combine.query<const SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
        auto& finalResults = *combine.query<SharedRow<SweepNPruneBroadphase::PairChanges>>().tryGetSingletonElement();

        combine.setCallback([&finalResults, data, grid](AppTaskArgs&) mutable {
          finalResults.mGained.clear();
          finalResults.mLost.clear();

          Broadphase::processPendingRemovals(grid->objects, finalResults.mLost);

          //First update all the reference counts based on individual reports of gains and losses within cells
          for(size_t i = 0; i < data->tasks.size(); ++i) {
             const TaskData& t = data->tasks[i];
            //Remove duplicates while also ignoring any that show up in both gained and lost
            for(const SweepCollisionPair& g : t.gains) {
              //Increment for gains, taking note of when new elements are created
              auto&& [it, _] = data->trackedPairs.emplace(g, Tasks::Tracker{});
              it->second.increment(1);
              if(it->second.currentRefCount == 1) {
                data->changedTrackers.emplace_back(&*it);
              }
            }
            for(const SweepCollisionPair& l : t.losses) {
              if(auto it = data->trackedPairs.find(l); it != data->trackedPairs.end()) {
                //Decrement for losses. Presumably pairs should always be found since reference counts are balanced
                it->second.increment(-1);
                if(it->second.currentRefCount == 0) {
                  data->changedTrackers.emplace_back(&*it);
                }
              }
            }
          }

          //Now go over all the changed pairs and resolve any duplication or consolidate matching add/remove pairs
          //The existence of the originalTracker optional indicates if this pair has been processed and the original
          //value can be used to determine if it's a new or removed pair
          //For example, if a pair is lost by one cell but gained by another, then it's not necessary to report either
          //gain or loss because the pair still exists
          for(auto& pair : data->changedTrackers) {
            //If this pair was already processed, skip it
            if(!pair->second.originalRefCount) {
              continue;
            }
            switch(pair->second.getAction()) {
              case Tasks::Tracker::Action::Add: finalResults.mGained.emplace_back(pair->first); break;
              case Tasks::Tracker::Action::Remove: finalResults.mLost.emplace_back(pair->first); break;
              default: break;
            }
            //Clearing the original reference marks this as processed so it is skipped if it comes up again
            pair->second.originalRefCount.reset();
          }
          //Final pass to delete. Can't be done in the iteration above as it may invalidate pointers that exist later in the container
          for(const SweepCollisionPair& loss : finalResults.mLost) {
            if(auto it = data->trackedPairs.find(loss); it != data->trackedPairs.end()) {
              data->trackedPairs.erase(it);
            }
          }

          data->changedTrackers.clear();
        });
        builder.submitTask(std::move(combine));
      }
    }
  }
}