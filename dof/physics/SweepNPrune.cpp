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

  namespace SweepNPrune {
    BroadphaseKey getOrCreateKey(std::vector<BroadphaseKey>& freeList, BroadphaseKey& newKey) {
      if(!freeList.empty()) {
        const BroadphaseKey result = freeList.back();
        freeList.pop_back();
        return result;
      }
      return { newKey.value++ };
    }

    void insertRange(Sweep2D& sweep,
      const UserKey* userKeys,
      BroadphaseKey* outKeys,
      size_t count) {
      BroadphaseKey newKey = { sweep.userKey.size() };
      if(count > sweep.freeList.size()) {
        const size_t slotsNeeded = count - sweep.freeList.size();
        for(size_t s = 0; s < Sweep2D::S; ++s) {
          sweep.bounds[s].resize(sweep.bounds[s].size() + slotsNeeded);
        }
        sweep.userKey.resize(sweep.userKey.size() + slotsNeeded);
      }
      //Axis doesn't use the free list, always add to end
      size_t newAxisIndex = sweep.axis[0].elements.size();
      for(size_t s = 0; s < Sweep2D::S; ++s) {
        sweep.axis[s].elements.resize(sweep.axis[s].elements.size() + (count * 2));
      }

      for(size_t i = 0; i < count; ++i) {
        const BroadphaseKey k = getOrCreateKey(sweep.freeList, newKey);

        outKeys[i] = { k };
        sweep.userKey[k.value] = userKeys[i];

        for(size_t d = 0; d < sweep.axis.size(); ++d) {
          sweep.bounds[d][k.value] = { Sweep2D::NEW, Sweep2D::NEW };
          //These are put at the end now and will be addressed in the next recomputePairs
          sweep.axis[d].elements[newAxisIndex] = SweepElement::createBegin(k);
          sweep.axis[d].elements[newAxisIndex + 1] = SweepElement::createEnd(k);
        }
        newAxisIndex += 2;
      }
    }

    void eraseRange(Sweep2D& sweep,
      const BroadphaseKey* keys,
      size_t count) {
      //Put this key in the pending removal list, not yet the free list so it doesn't get re-used
      //This allows the bounds to be written as out of bounds and then for the next recompute to process
      //the removals rather than having to do a synchronous search here
      const size_t base = sweep.pendingRemoval.size();
      sweep.pendingRemoval.resize(sweep.pendingRemoval.size() + count);

      for(size_t i = 0; i < count; ++i) {
        const BroadphaseKey& k = keys[i];
        sweep.pendingRemoval[base + i] = k;

        //Write the new bounds so that the key is out of the way
        for(size_t d = 0; d < Sweep2D::S; ++d) {
          sweep.bounds[d][k.value] = { Sweep2D::REMOVED, Sweep2D::REMOVED };
        }
        //User key needs to be left as-is so it can show up for removal of pairs
      }
    }

    void updateBoundaries(Sweep2D& sweep,
      const float* minX,
      const float* maxX,
      const float* minY,
      const float* maxY,
      const BroadphaseKey* keys,
      size_t count) {
      for(size_t i = 0; i < count; ++i) {
        const size_t k = keys[i].value;
        sweep.bounds[0][k] = { minX[i], maxX[i] };
        sweep.bounds[1][k] = { minY[i], maxY[i] };
      }
    }

    void recomputeCandidates(Sweep2D& sweep, CollisionCandidates& candidates) {
      PROFILE_SCOPE("physics", "computeCandidates");

      for(size_t i = 0; i < Sweep2D::S; ++i) {
        insertionSort(sweep.axis[i].elements.data(), sweep.axis[i].elements.data() + sweep.axis[i].elements.size(), sweep.bounds[i].data(), candidates);
      }
      //All removal events should have been logged, now insert removals into free list and trim them off the end of the axes
      //Removals would always be at the end because REMOVED is float max and it was just sorted above
      if(size_t toRemove = sweep.pendingRemoval.size()) {
        //Log an event for all removal pairs. This is because all removal bounds are equal so their events might be missed if both were removed
        //resolveCandidates will remove redundancies
        for(size_t i = 0; i < sweep.pendingRemoval.size(); ++i) {
          for(size_t j = i + 1; j < sweep.pendingRemoval.size(); ++j) {
            candidates.pairs.emplace_back(sweep.pendingRemoval[i].value, sweep.pendingRemoval[j].value);
          }
        }
        for(size_t i = 0; i < Sweep2D::S; ++i) {
          sweep.axis[i].elements.resize(sweep.axis[i].elements.size() - sweep.pendingRemoval.size()*2);
        }
        sweep.freeList.insert(sweep.freeList.end(), sweep.pendingRemoval.begin(), sweep.pendingRemoval.end());
        sweep.pendingRemoval.clear();
      }
    }

    void resolveCandidates(Sweep2D& sweep, CollisionCandidates& collisionCandidates, SwapLog log) {
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
          if(!isOverlapping(sweep.bounds[d][candidate.a], sweep.bounds[d][candidate.b])) {
            isColliding = false;
            break;
          }
        }
        const SweepCollisionPair userPair{ sweep.userKey[candidate.a], sweep.userKey[candidate.b] };
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

    void recomputePairs(Sweep2D& sweep, CollisionCandidates& candidates, SwapLog& log) {
      recomputeCandidates(sweep, candidates);
      resolveCandidates(sweep, candidates, log);
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
      BroadphaseKey newKey{ grid.mappings.mappings.size() };
      if(count > grid.freeList.size()) {
        grid.mappings.mappings.resize(grid.mappings.mappings.size() + count - grid.freeList.size());
        grid.mappings.userKeys.resize(grid.mappings.mappings.size());
      }
      //Assign the keys a slot but don't map them to any internal cells yet since bounds aren't known
      //This will happen during updateBoundaries
      for(size_t i = 0; i < count; ++i) {
        const BroadphaseKey key = SweepNPrune::getOrCreateKey(grid.freeList, newKey);
        grid.mappings.mappings[key.value] = {};
        grid.mappings.userKeys[key.value] = userKeys[i];
        outKeys[i] = key;
      }
    }

    void eraseRange(Grid& grid,
      const BroadphaseKey* keys,
      size_t count) {
      size_t freeIndex = grid.freeList.size();
      grid.freeList.resize(grid.freeList.size() + count);
      for(size_t i = 0; i < count; ++i) {
        //Use the mapping to remove from the internal cell
        KeyMapping mapping = grid.mappings.mappings[keys[i].value];
        for(CellKey& key : mapping.publicToPrivate) {
          if(key.cellKey.value != EMPTY) {
            Sweep2D& cell = grid.cells[key.cellKey.value];
            SweepNPrune::eraseRange(cell, &key.elementKey, 1);
            //Clear the mapping
            key = {};
          }
        }
        //Add the mapping to the free list
        grid.freeList[freeIndex++] = keys[i];
        grid.mappings.userKeys[keys[i].value] = EMPTY;
      }
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

    //Find all cells that the bounds overlap with. This assumes nothing could ever be big enough to overlap with more than 4
    KeyMapping getKey(const GridDefinition& definition, const Bounds& bounds) {
      //Translate to local space such that each cell is of size 1 and the bottom left cell is at the origin
      const glm::vec2 invScale{ 1.0f/definition.cellSize.x, 1.0f/definition.cellSize.y };
      const glm::vec2 boundaryMin = glm::vec2(0, 0);
      const glm::vec2 boundaryMax = glm::vec2(definition.cellsX - 1, definition.cellsY - 1);
      const glm::vec2 localMin = glm::clamp((bounds.min - definition.bottomLeft)*invScale, boundaryMin, boundaryMax);
      const glm::vec2 localMax = glm::clamp((bounds.max - definition.bottomLeft)*invScale, boundaryMin, boundaryMax);
      //Now that they are in local space truncation to int can be used to find all the desired indices
      int found = 0;
      KeyMapping result;
      for(int x = static_cast<int>(localMin.x); x <= static_cast<int>(localMax.x); ++x) {
        for(int y = static_cast<int>(localMin.y); y <= static_cast<int>(localMax.y); ++y) {
          const BroadphaseKey key{ x + y*definition.cellsX };
          result.publicToPrivate[found++].cellKey = key;
          //Shouldn't generally happen but would be if an object spans more than 4 cells.
          //Either missing the collision needs to be okay or objects should never get that big
          if(found >= result.publicToPrivate.size()) {
            break;
          }
        }
      }
      return result;
    }

    void removeMapping(KeyMapping& key, size_t i) {
      //Shift all elements down
      //Swap remove would be fine too but still requires finding where the last element is
      for(size_t j = i; j + 1 < key.publicToPrivate.size(); ++j) {
        key.publicToPrivate[j] = key.publicToPrivate[j + 1];
      }
      key.publicToPrivate.back() = {};
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
        const Bounds newBounds{ glm::vec2{ minX[i], minY[i] }, glm::vec2{ maxX[i], maxY[i] } };
        KeyMapping& currentMapping = grid.mappings.mappings[keys[i].value];
        KeyMapping desiredMapping = getKey(grid.definition, newBounds);
        //Resolve differences between current and desired. Common case should be that there are none
        //Start with finding keys that already match and removing those that don't
        size_t c = 0;
        for(c = 0; c < currentMapping.publicToPrivate.size();) {
          CellKey& current = currentMapping.publicToPrivate[c];
          if(current.cellKey == EMPTY_KEY) {
            //Empty entries come last so if there's an empty one no need to continue
            break;
          }
          bool foundCurrent = false;
          for(CellKey& found : desiredMapping.publicToPrivate) {
            if(current.cellKey == found.cellKey) {
              found = {};
              foundCurrent = true;
              break;
            }
          }
          if(foundCurrent) {
            ++c;
          }
          else {
            Sweep2D& cell = grid.cells[current.cellKey.value];
            SweepNPrune::eraseRange(cell, &current.elementKey, 1);
            removeMapping(currentMapping, c);
          }
        }

        //Now any remaining keys are new ones that need to be added
        for(const CellKey& newKey : desiredMapping.publicToPrivate) {
          if(newKey.cellKey != EMPTY_KEY && c < currentMapping.publicToPrivate.size()) {
            CellKey& slot = currentMapping.publicToPrivate[c++];
            slot.cellKey = newKey.cellKey;
            SweepNPrune::insertRange(grid.cells[slot.cellKey.value], &grid.mappings.userKeys[keys[i].value], &slot.elementKey, 1);
          }
        }

        //Now actually write the bounds. This could be split into a separate step if the grid itself kept a copy of all bounds
        for(size_t e = 0; e < c; ++e) {
          const Broadphase::SweepGrid::CellKey& key = currentMapping.publicToPrivate[e];
          SweepNPrune::updateBoundaries(grid.cells[key.cellKey.value],
            minX + i,
            maxX + i,
            minY + i,
            maxY + i,
            &key.elementKey, 1);
        }
      }
    }

    void recomputePairs(IAppBuilder& builder) {
      struct TaskData {
        std::vector<Broadphase::SweepCollisionPair> gains, losses;
        Broadphase::CollisionCandidates candidates;
      };
      struct Tasks {
        std::vector<TaskData> tasks;
        //Hack to reference count pairs to avoid duplicate events for pairs in multiple cells
        std::unordered_map<Broadphase::SweepCollisionPair, uint8_t> trackedPairs;
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
        //Artificial dependency on grid so the previous finishes first
        combine.query<const SharedRow<Broadphase::SweepGrid::Grid>>();
        auto& finalResults = *combine.query<SharedRow<SweepNPruneBroadphase::PairChanges>>().tryGetSingletonElement();

        combine.setCallback([&finalResults, data](AppTaskArgs&) mutable {
          finalResults.mGained.clear();
          finalResults.mLost.clear();

          for(size_t i = 0; i < data->tasks.size(); ++i) {
             const TaskData& t = data->tasks[i];
            //Remove duplicates while also ignoring any that show up in both gained and lost
            for(const SweepCollisionPair& g : t.gains) {
              //If this was a new entry, report the gain
              if(!data->trackedPairs[g]++) {
                finalResults.mGained.push_back(g);
              }
            }
            for(const SweepCollisionPair& l : t.losses) {
              if(auto it = data->trackedPairs.find(l); it != data->trackedPairs.end()) {
                //If reference count hits zero, report the loss and remove the tracking element
                if(!it->second || !--it->second) {
                  data->trackedPairs.erase(it);
                  finalResults.mLost.push_back(l);
                }
              }
            }
          }
        });
        builder.submitTask(std::move(combine));
      }
    }
  }
}