#include "Precompile.h"
#include "SweepNPrune.h"

#include <cassert>

#include "glm/common.hpp"
#include "Scheduler.h"
#include "Profile.h"

namespace Broadphase {
  struct ElementBounds {
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

  bool isOverlapping(const ElementBounds& l, const ElementBounds& r) {
    if(l.bounds.second < r.bounds.first) {
      //L entirely less than R
      return false;
    }
    if(l.bounds.first > r.bounds.second) {
      //L entirely greater than R
      return false;
    }
    return true;
  }

  //Left is a start element moving back over right
  void logSwapBackStart(const ElementBounds& left, SweepElement right, const std::pair<float, float>* bounds, SwapLog& log) {
    if(right.isStart()) {
      //Left start passed over right start meaning left end might still be within it, nothing to do yet
    }
    else {
      //Left start passed over right end, starting to overlap with right edge
      const auto r = unwrapElement(right, bounds);
      if(isOverlapping(left, r)) {
        log.gains.emplace_back(BroadphaseKey{ left.element.getValue() }, BroadphaseKey{ r.element.getValue() });
      }
    }
  }

  //Left is an end element moving back over right
  void logSwapBackEnd(const ElementBounds& left, SweepElement right, const std::pair<float, float>* bounds, SwapLog& log) {
    if(right.isEnd()) {
      //Left end moved over right end meaning left end might still be within right start, nothing to do yet
    }
    else {
      //Left end swapped over right begin, overlap may have ended
      const auto r = unwrapElement(right, bounds);
      if(!isOverlapping(left, r)) {
        log.losses.emplace_back(BroadphaseKey{ left.element.getValue() }, BroadphaseKey{ r.element.getValue() });
      }
    }
  }

  void insertionSort(SweepElement* first,
    SweepElement* last,
    const std::pair<float, float>* bounds,
    SwapLog& log) {
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
      if(valueBounds.value < firstBounds.value) {
        SweepElement* current = mid - 1;
        //Swap this element with all between first and mid, shifting right to make space
        while(current != first) {
          logSwap(valueBounds, *current, bounds, log);
          *(current + 1) = *current;
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
          if(c.value < valueBounds.value) {
            break;
          }

          logSwap(valueBounds, *current, bounds, log);
          *(current + 1) = *current;
          --current;
        }
        //Insert element at new insertion point
        *current = value;
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
      size_t newAxisIndex = sweep.axis[0].elements.size();
      if(count > sweep.freeList.size()) {
        const size_t slotsNeeded = count - sweep.freeList.size();
        for(size_t s = 0; s < Sweep2D::S; ++s) {
          sweep.bounds[s].resize(sweep.bounds[s].size() + slotsNeeded);
          sweep.axis[s].elements.resize(sweep.axis[s].elements.size() + (slotsNeeded * 2));
        }
        sweep.userKey.resize(sweep.userKey.size() + slotsNeeded);
      }

      BroadphaseKey newKey = { sweep.bounds[0].size() };
      for(size_t i = 0; i < count; ++i) {
        const BroadphaseKey k = getOrCreateKey(sweep.freeList, newKey);

        outKeys[i] = { k };
        sweep.userKey[k.value] = userKeys[i];

        for(size_t d = 0; d < sweep.axis.size(); ++d) {
          sweep.bounds[d][k.value] = { Sweep2D::NEW, Sweep2D::NEW };
          //These are put at the end now and will be addressed in the next recomputePairs
          sweep.axis[d].elements[newAxisIndex++] = SweepElement::createBegin(k);
          sweep.axis[d].elements[newAxisIndex++] = SweepElement::createEnd(k);
        }
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

    void recomputePairs(Sweep2D& sweep, SwapLog& log) {
      for(size_t i = 0; i < Sweep2D::S; ++i) {
        insertionSort(sweep.axis[i].elements.data(), sweep.axis[i].elements.data() + sweep.axis[i].elements.size(), sweep.bounds[i].data(), log);
      }
      //All removal events should have been logged, now insert removals into free list
      sweep.freeList.insert(sweep.freeList.end(), sweep.pendingRemoval.begin(), sweep.pendingRemoval.end());
      sweep.pendingRemoval.clear();
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
        grid.mappings.mappings.resize(count - grid.freeList.size());
        grid.mappings.userKeys.reserve(grid.mappings.mappings.size());
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
      const glm::vec2 localMin = glm::max(glm::vec2(0, 0), (bounds.min - definition.bottomLeft)*invScale);
      const glm::vec2 localMax = glm::min(glm::vec2(definition.cellsX, definition.cellsY), (bounds.max - definition.bottomLeft)*invScale);
      //Now that they are in local space truncation to int can be used to find all the desired indices
      int found = 0;
      KeyMapping result;
      for(int x = static_cast<int>(localMin.x); x <= static_cast<int>(localMax.x); ++x) {
        for(int y = static_cast<int>(localMax.y); y <= static_cast<int>(localMax.y); ++y) {
          const BroadphaseKey key{ x + y*definition.cellsX };
          result.publicToPrivate[found].cellKey = key;
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
            SweepNPrune::eraseRange(cell, &current.cellKey, 1);
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
          SweepNPrune::updateBoundaries(grid.cells[currentMapping.publicToPrivate[e].cellKey.value],
            minX + i,
            maxX + i,
            minY + i,
            maxY + i,
            keys + i, 1);
        }
      }
    }

    void recomputePairs(Grid& grid, SwapLog& log) {
      for(Sweep2D& sweep : grid.cells) {
        SweepNPrune::recomputePairs(sweep, log);
      }
    }

    TaskRange recomputePairs(Grid& grid, SwapLog finalResults) {
      struct TaskData {
        std::vector<Broadphase::SweepCollisionPair> gains, losses;
      };
      struct Tasks {
        std::vector<TaskData> tasks;
        std::unordered_set<Broadphase::SweepCollisionPair> combinedGains, combinedLosses;
      };
      auto data = std::make_shared<Tasks>();
      auto processOne = TaskNode::create([data, &grid](enki::TaskSetPartition range, uint32_t) {
        PROFILE_SCOPE("physics", "recomputePairs");
        for(size_t i = range.start; i < range.end; ++i) {
          SwapLog log {
            data->tasks[i].gains,
            data->tasks[i].losses
          };
          log.gains.clear();
          log.losses.clear();
          SweepNPrune::recomputePairs(grid.cells[i], log);
        }
      });
      auto configureTasks = TaskNode::create([processOne, data, &grid](...) {
        static_cast<enki::TaskSet*>(processOne->mTask.get())->m_SetSize = grid.cells.size();
        data->tasks.resize(grid.cells.size());
      });
      auto combineResults = TaskNode::create([data, finalResults](...) {
        PROFILE_SCOPE("physics", "combineResults");
        data->combinedGains.clear();
        data->combinedLosses.clear();
        for(const TaskData& t : data->tasks) {
          //Remove duplicates while also ignoring any that show up in both gained and lost
          data->combinedGains.insert(t.gains.begin(), t.gains.end());
          for(const auto& l : t.losses) {
            if(auto it = data->combinedGains.find(l); it != data->combinedGains.end()) {
              data->combinedGains.erase(it);
            }
            else {
              data->combinedLosses.insert(l);
            }
          }
        }

        finalResults.gains.clear();
        finalResults.losses.clear();
        finalResults.gains.insert(finalResults.gains.end(), data->combinedGains.begin(), data->combinedGains.end());
        finalResults.losses.insert(finalResults.losses.end(), data->combinedLosses.begin(), data->combinedLosses.end());
      });

      auto root = configureTasks;
      configureTasks->mChildren.push_back(processOne);
      processOne->mChildren.push_back(combineResults);
      return TaskBuilder::addEndSync(root);
    }
  }

}