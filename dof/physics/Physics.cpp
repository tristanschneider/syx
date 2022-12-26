#include "Precompile.h"
#include "Physics.h"

#include "TableOperations.h"
#include "glm/common.hpp"

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
    const size_t cx = glm::clamp(x, dimensions.mOriginX, int(dimensions.mCellsX));
    const size_t cy = glm::clamp(y, dimensions.mOriginY, int(dimensions.mCellsY));
    return size_t(cx - dimensions.mOriginX) + size_t(cy - dimensions.mOriginY)*dimensions.mCellsX;
  }

  void _addCollisionPair(size_t self, size_t other, CollisionPairsTable& table) {
    //Always add pairs in the same order (>) so that if a pair is added as a b then b a it will be ignored as duplicate
    //if(self > other) {
    //  std::swap(self, other);
    //}
    CollisionPairsTable::ElementRef result = TableOperations::addToSortedTable<CollisionPairIndexA>(table, self);
    result.get<1>() = other;
  }
}

void Physics::allocateBroadphase(GridBroadphase::BroadphaseTable& table) {
  const auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(table.mRows).at();
  auto& allocatedDimensions = std::get<SharedRow<GridBroadphase::AllocatedDimensions>>(table.mRows).at();
  constexpr float cellSize = 1.0f;
  assert(dimensions.mMin.x <= dimensions.mMax.x);
  assert(dimensions.mMin.y <= dimensions.mMax.y);

  const IRect rect = _buildRect(dimensions.mMin, dimensions.mMax);

  allocatedDimensions.mOriginX = size_t(rect.mMin.x);
  allocatedDimensions.mOriginY = size_t(rect.mMin.y);
  allocatedDimensions.mCellsX = std::max(size_t(1), size_t(rect.mMax.x) - allocatedDimensions.mOriginX);
  allocatedDimensions.mCellsY = std::max(size_t(1), size_t(rect.mMax.y) - allocatedDimensions.mOriginY);

  //Allocate desired space
  TableOperations::resizeTable(table, allocatedDimensions.mCellsY * allocatedDimensions.mCellsX);
  clearBroadphase(table);
}

void Physics::rebuildBroadphase(
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
  const glm::vec2 extents(std::sqrt(centerToEdge*centerToEdge)*2);
  for(size_t i = 0; i < insertCount; ++i) {
    const glm::vec2 center{ xPositions[i], yPositions[i] };
    const IRect rect = _buildRect(center - extents, center + extents);
    const size_t indexToStore = baseIndex + i;

    //Store index to this in all cells it overlaps with
    for(int x = rect.mMin.x; x < rect.mMax.x; ++x) {
      for(int y = rect.mMin.y; y < rect.mMax.y; ++y) {
        bool slotFound = false;
        for(size_t& storedIndex : cells[_toIndex(x, y, dimensions)].mElements) {
          if(storedIndex == GridBroadphase::EMPTY_ID) {
            storedIndex = indexToStore;
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

void Physics::clearBroadphase(GridBroadphase::BroadphaseTable& broadphase) {
  for(GridBroadphase::Cell& cell : std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements) {
    cell.mElements.fill(GridBroadphase::EMPTY_ID);
  }
}

//TODO: this is a bit clunky to do separately from generation and update because most of the time all collision pairs from last frame would have been fine
void Physics::generateCollisionPairs(const GridBroadphase::BroadphaseTable& broadphase, CollisionPairsTable& pairs) {
  const GridBroadphase::Overflow& overflow = std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at();

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
          continue;
        }
        //TODO: optimization here for avoiding pairs of static objects and for doing all inserts at once
        _addCollisionPair(selfID, otherID, pairs);
      }

      //Add pairs with cell-less overflow objects
      //The hope is that overflow is rare/never happens
      for(size_t other : overflow.mElements) {
        if(other != selfID) {
          _addCollisionPair(selfID, other, pairs);
        }
      }
    }
  }
}
