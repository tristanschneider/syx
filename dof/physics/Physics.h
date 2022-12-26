#pragma once

#include "glm/vec2.hpp"
#include "Queries.h"
#include "Table.h"

//Grid broadphase optimized for an expected object size.
//This assumes that up to the configured amount of objects will fit into a given grid cell,
//allowing all cells to have simple staticly allocated storage
struct GridBroadphase {
  static constexpr size_t EMPTY_ID = std::numeric_limits<size_t>::max();
  struct Cell {
    //These are GameDatabase::ElementIDs
    //4 is the maximum expected amount of expected objects that could fit in a cell
    //If the cell is full they will go into overflow
    std::array<size_t, 4> mElements;
  };
  //Broadphase degrades into n^2 lookup if cells are full
  struct Overflow {
    std::vector<size_t> mElements;
  };
  struct RequestedDimensions {
    glm::ivec2 mMin, mMax;
  };
  struct AllocatedDimensions {
    int mOriginX{};
    int mOriginY{};
    size_t mCellsX{};
    size_t mCellsY{};
  };

  using BroadphaseTable = Table<
    Row<GridBroadphase::Cell>,
    SharedRow<GridBroadphase::Overflow>,
    //Configured by user to match desired dimensions
    SharedRow<GridBroadphase::RequestedDimensions>,
    //Configured by implementation based on RequestedDimensions
    SharedRow<GridBroadphase::AllocatedDimensions>
  >;
};

struct CollisionPairIndexA : Row<size_t> {};
struct CollisionPairIndexB : Row<size_t> {};

template<class>
struct NarrowphaseData {
  struct PosX : Row<float>{};
  struct PosY : Row<float>{};
  struct CosAngle : Row<float>{};
  struct SinAngle : Row<float>{};
};

struct PairA{};
struct PairB{};

//Sorted by indexA
using CollisionPairsTable = Table<
  CollisionPairIndexA,
  CollisionPairIndexB,
  //These rows are generated and updated by the broadphase and carried through to narrowphase
  //It is convenient here because it's already sorted by indexA which allows for some reduction in fetches
  //to populate the data.
  //If the cost of the broadphase inserts becomes too high due to shuffling these rows then the data could be
  //split off to its own table
  NarrowphaseData<PairA>::PosX,
  NarrowphaseData<PairA>::PosY,
  NarrowphaseData<PairA>::CosAngle,
  NarrowphaseData<PairA>::SinAngle,

  NarrowphaseData<PairB>::PosX,
  NarrowphaseData<PairB>::PosY,
  NarrowphaseData<PairB>::CosAngle,
  NarrowphaseData<PairB>::SinAngle
>;

struct Physics {
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

  static void generateCollisionPairs(const GridBroadphase::BroadphaseTable& broadphase, CollisionPairsTable& pairs);

  struct details {
    template<class SrcRow, class DstRow, class DatabaseT>
    static void fillRow(CollisionPairsTable& pairs, DatabaseT& db, std::vector<size_t>& ids) {
      DstRow& dst = std::get<DstRow>(pairs.mRows);
      SrcRow* src = nullptr;
      DatabaseT::ElementID last;
      for(size_t i = 0; i < ids.size(); ++i) {
        const DatabaseT::ElementID id(ids[i]);
        //Retreive the rows every time the tables change, which should be rarely
        if(!src || last.getTableIndex() != id.getTableIndex()) {
          src = Queries::getRowInTable<SrcRow>(db, id);
        }

        if(src) {
          dst.at(i) = src->at(id.getElementIndex());
        }

        last = id;
      }
    }
  };

  //Populates narrowphase data by fetching it from the provided input using the indices stored by the broadphase
  template<class PosX, class PosY, class CosAngle, class SinAngle, class DatabaseT>
  static void fillNarrowphaseData(CollisionPairsTable& pairs, DatabaseT& db) {
    std::vector<size_t>& idsA = std::get<CollisionPairIndexA>(pairs.mRows).mElements;
    details::fillRow<PosX, NarrowphaseData<PairA>::PosX>(pairs, db, idsA);
    details::fillRow<PosY, NarrowphaseData<PairA>::PosY>(pairs, db, idsA);
    details::fillRow<CosAngle, NarrowphaseData<PairA>::CosAngle>(pairs, db, idsA);
    details::fillRow<SinAngle, NarrowphaseData<PairA>::SinAngle>(pairs, db, idsA);

    std::vector<size_t>& idsB = std::get<CollisionPairIndexB>(pairs.mRows).mElements;
    details::fillRow<PosX, NarrowphaseData<PairB>::PosX>(pairs, db, idsB);
    details::fillRow<PosY, NarrowphaseData<PairB>::PosY>(pairs, db, idsB);
    details::fillRow<CosAngle, NarrowphaseData<PairB>::CosAngle>(pairs, db, idsB);
    details::fillRow<SinAngle, NarrowphaseData<PairB>::SinAngle>(pairs, db, idsB);
  }
};