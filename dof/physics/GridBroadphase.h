#pragma once

#include "NarrowphaseData.h"
#include "glm/vec2.hpp"
#include "Table.h"

struct PhysicsTableIds;

//Grid broadphase optimized for an expected object size.
//This assumes that up to the configured amount of objects will fit into a given grid cell,
//allowing all cells to have simple staticly allocated storage
struct GridBroadphase {
  static constexpr size_t EMPTY_ID = std::numeric_limits<size_t>::max();
  struct Cell {
    //These are GameDatabase::ElementIDs
    //TODO: this doesn't seem to work well at all, it overflows so easily even with much space
    //4 is the maximum expected amount of expected objects that could fit in a cell
    //If the cell is full they will go into overflow
    std::array<size_t, 10> mElements;
  };
  //Broadphase degrades into n^2 lookup if cells are full
  struct Overflow {
    std::vector<size_t> mElements;
  };
  struct RequestedDimensions {
    glm::ivec2 mMin, mMax;
  };
  struct AllocatedDimensions {
    glm::ivec2 mMin, mMax;
    size_t mStride{};
  };

  using BroadphaseTable = Table<
    Row<GridBroadphase::Cell>,
    SharedRow<GridBroadphase::Overflow>,
    //Configured by user to match desired dimensions
    SharedRow<GridBroadphase::RequestedDimensions>,
    //Configured by implementation based on RequestedDimensions
    SharedRow<GridBroadphase::AllocatedDimensions>
  >;

  //Given the Dimensions stored in the table allocates the space for the grid
  //The caller must call this before using the table for any queries below
  static void allocateBroadphase(GridBroadphase::BroadphaseTable& table);

  //Given the input arrays of positions and cells, fills cells with the elements at those positions
  //Elements are stored in the provided cells, the caller must ensure that there are the same amounts of
  //position elements as cells. The ids stored in the cells are baseIndex + the pointer offset,
  //which would map to the caller's Database::ElementID type
  static void rebuildBroadphase(
    size_t baseIndex,
    const float* xPositions,
    const float* yPositions,
    GridBroadphase::BroadphaseTable& broadphase,
    size_t insertCount);

  static void clearBroadphase(GridBroadphase::BroadphaseTable& broadphase);

  static void generateCollisionPairs(const GridBroadphase::BroadphaseTable& broadphase, CollisionPairsTable& pairs, const PhysicsTableIds& tableIds);
};
