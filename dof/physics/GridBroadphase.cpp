#include "Precompile.h"

#include "GridBroadphase.h"

#include "PhysicsTableIds.h"
#include "glm/common.hpp"
#include "glm/detail/func_geometric.inl"
#include "TableOperations.h"

namespace {
  struct IRect {
    glm::ivec2 mMin, mMax;
  };

  IRect _buildRect(const glm::vec2& min, const glm::vec2& max) {
    return {
      glm::ivec2{ int(std::floor(min.x)), int(std::floor(min.y)) },
      glm::ivec2{ int(std::ceil(max.x)), int(std::ceil(max.y)) }
    };
  }

  size_t _toIndex(int x, int y, const GridBroadphase::AllocatedDimensions& dimensions) {
    const int cx = glm::clamp(x, dimensions.mMin.x, dimensions.mMax.x);
    const int cy = glm::clamp(y, dimensions.mMin.y, dimensions.mMax.y);
    return size_t(cx - dimensions.mMin.x) + size_t(cy - dimensions.mMin.y)*dimensions.mStride;
  }

  bool _tryOrderCollisionPair(size_t& pairA, size_t& pairB, const PhysicsTableIds& tables) {
    //Always make zero mass object 'B' in a pair for simplicity
    if((pairA & tables.mTableIDMask) == tables.mZeroMassTable) {
      if((pairB & tables.mTableIDMask) == tables.mZeroMassTable) {
        //If they're both static, skip this pair
        return false;
      }
      std::swap(pairA, pairB);
    }
    return true;
  }

  void _addCollisionPair(size_t self, size_t other, CollisionPairsTable& table, const PhysicsTableIds& tableIds) {
    if(!_tryOrderCollisionPair(self, other, tableIds)) {
      return;
    }

    CollisionPairIndexA& rowA = std::get<CollisionPairIndexA>(table.mRows);
    CollisionPairIndexB& rowB = std::get<CollisionPairIndexB>(table.mRows);
    //Table is sorted by index A, so find A first
    auto it = std::lower_bound(rowA.begin(), rowA.end(), self);
    if(it != rowA.end() && *it == self) {
      //If A was found, start here in B to see if that index exists
      const size_t indexA = std::distance(rowA.begin(), it);
      for(size_t i = indexA; i < rowA.size(); ++i) {
        //If this passed the range of A indices then stop now, this is not a duplicate
        if(rowA.at(i) != self) {
          break;
        }
        //If B was found, that means the pair of A and B already exists, so no need to add it
        if(rowB.at(i) == other) {
          return;
        }
      }
    }

    //Insert only in this row here, at the end of collision pair gathering there will be a single resize to realign all the rows
    size_t insertIndex = size_t(std::distance(rowA.begin(), it));
    rowA.mElements.insert(it, self);
    rowB.mElements.insert(rowB.begin() + insertIndex, other);
  }
}

void GridBroadphase::allocateBroadphase(GridBroadphase::BroadphaseTable& table) {
  const auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(table.mRows).at();
  auto& allocatedDimensions = std::get<SharedRow<GridBroadphase::AllocatedDimensions>>(table.mRows).at();
  constexpr float cellSize = 1.0f;
  assert(dimensions.mMin.x <= dimensions.mMax.x);
  assert(dimensions.mMin.y <= dimensions.mMax.y);

  const IRect rect = _buildRect(dimensions.mMin, dimensions.mMax);

  allocatedDimensions.mMin = rect.mMin;
  allocatedDimensions.mMax = rect.mMax;
  allocatedDimensions.mStride = std::max(size_t(1), size_t(rect.mMax.x - rect.mMin.x));
  size_t sizeY = size_t(allocatedDimensions.mMax.y - allocatedDimensions.mMin.y) + size_t(1);

  //Allocate desired space
  TableOperations::resizeTable(table, sizeY * allocatedDimensions.mStride);
  clearBroadphase(table);
}

void GridBroadphase::rebuildBroadphase(
  size_t baseIndex,
  const float* xPositions,
  const float* yPositions,
  GridBroadphase::BroadphaseTable& broadphase,
  size_t insertCount) {
  const auto& dimensions = std::get<SharedRow<GridBroadphase::AllocatedDimensions>>(broadphase.mRows).at();
  auto& overflow = std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at();
  std::vector<GridBroadphase::Cell>& cells = std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements;

  //Vector to max extents of the shape regardless of rotation
  const float centerToEdge = 0.5f;
  const glm::vec2 extents(std::sqrt(centerToEdge*centerToEdge*2));
  for(size_t i = 0; i < insertCount; ++i) {
    const glm::vec2 center{ xPositions[i], yPositions[i] };
    const IRect rect = _buildRect(center - extents, center + extents);
    const size_t indexToStore = baseIndex + i;

    //Store index to this in all cells it overlaps with
    for(int x = rect.mMin.x; x < rect.mMax.x; ++x) {
      for(int y = rect.mMin.y; y < rect.mMax.y; ++y) {
        bool slotFound = false;
        const size_t cellIndex = _toIndex(x, y, dimensions);
        for(size_t& storedIndex : cells[cellIndex].mElements) {
          if(storedIndex == GridBroadphase::EMPTY_ID) {
            storedIndex = indexToStore;
            slotFound = true;
            break;
          }
          //Don't put self in list multiple times. Shouldn't generally happen unless index is getting clamped due to being outside the boundaries
          else if(storedIndex == indexToStore) {
            slotFound = true;
            break;
          }
        }

        if(!slotFound) {
          overflow.mElements.push_back(indexToStore);
        }
      }
    }
  }

  //Remove duplicates
  std::sort(overflow.mElements.begin(), overflow.mElements.end());
  overflow.mElements.erase(std::unique(overflow.mElements.begin(), overflow.mElements.end()), overflow.mElements.end());
}

void GridBroadphase::clearBroadphase(GridBroadphase::BroadphaseTable& broadphase) {
  for(GridBroadphase::Cell& cell : std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements) {
    cell.mElements.fill(GridBroadphase::EMPTY_ID);
  }
  std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at().mElements.clear();
}

//TODO: this is a bit clunky to do separately from generation and update because most of the time all collision pairs from last frame would have been fine
void GridBroadphase::generateCollisionPairs(const GridBroadphase::BroadphaseTable& broadphase, CollisionPairsTable& pairs, const PhysicsTableIds& tableIds) {
  const GridBroadphase::Overflow& overflow = std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at();

  //TODO: retain pairs for a while then remove them occasionally if far away
  TableOperations::resizeTable(pairs, 0);

  //There could be a way to optimize for empty cells but the assumption is that most cells are not empty
  for(const GridBroadphase::Cell& cell : std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements) {
    for(size_t self = 0; self < cell.mElements.size(); ++self) {
      const size_t selfID = cell.mElements[self];
      if(selfID == GridBroadphase::EMPTY_ID) {
        continue;
      }
      //Add pairs with all others in the cell
      for(size_t other = self + 1; other < cell.mElements.size(); ++other) {
        const size_t otherID = cell.mElements[other];
        if(otherID == GridBroadphase::EMPTY_ID) {
          break;
        }
        //TODO: optimization here for avoiding pairs of static objects and for doing all inserts at once
        _addCollisionPair(selfID, otherID, pairs, tableIds);
      }

      //Add pairs with cell-less overflow objects
      //The hope is that overflow is rare/never happens
      for(size_t other : overflow.mElements) {
        if(other != selfID) {
          _addCollisionPair(selfID, other, pairs,tableIds);
        }
      }
    }
  }
  //_addCollisionPair modifies the index rows by themselves, after which this single resize fills in the rest of the rows
  TableOperations::resizeTable(pairs, std::get<CollisionPairIndexA>(pairs.mRows).size());
}