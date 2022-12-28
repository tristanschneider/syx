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

template<class>
struct ContactPoint {
  //Position of contact point in local space on object A
  struct PosX : Row<float> {};
  struct PosY : Row<float> {};
  //Overlap along shared normal
  struct Overlap : Row<float> {};
};
//Up to two contact points allows for a collision pair, which is plenty for 2d
struct ContactOne {};
struct ContactTwo {};

struct SharedNormal {
  struct X : Row<float> {};
  struct Y : Row<float> {};
};

//Sorted by indexA
using CollisionPairsTable = Table<
  CollisionPairIndexA,
  CollisionPairIndexB,
  //These rows are generated and updated by the broadphase and carried through to narrowphase
  //It is convenient here because it's already sorted by indexA which allows for some reduction in fetches
  //to populate the data.
  //If the cost of the broadphase inserts becomes too high due to shuffling these rows then the data could be
  //split off to its own table
  //This is a useful place to persist collision pair related data that needs to last to next frame,
  //like contact points and warm starts, data like that needs to remain sorted as collision pairs are inserted
  //Narrowphase data is regenerated every frame so doesn't have to be, and an optimization would be to only
  //sort the persistent rows then do a final resize on the non-persistent ones
  NarrowphaseData<PairA>::PosX,
  NarrowphaseData<PairA>::PosY,
  NarrowphaseData<PairA>::CosAngle,
  NarrowphaseData<PairA>::SinAngle,

  NarrowphaseData<PairB>::PosX,
  NarrowphaseData<PairB>::PosY,
  NarrowphaseData<PairB>::CosAngle,
  NarrowphaseData<PairB>::SinAngle,

  ContactPoint<ContactOne>::PosX,
  ContactPoint<ContactOne>::PosY,
  ContactPoint<ContactOne>::Overlap,

  ContactPoint<ContactTwo>::PosX,
  ContactPoint<ContactTwo>::PosY,
  ContactPoint<ContactTwo>::Overlap,

  SharedNormal::X,
  SharedNormal::Y
>;

//Data for one object in a constraint pair
template<class>
struct ConstraintObject {
  struct LinVelX : Row<float> {};
  struct LinVelY : Row<float> {};
  struct AngVel : Row<float> {};
  //If this is positive then before solving this element the values should be copied from
  //the indicated index
  //Necessary because some objects will inevitably exist in multiple different collision pairs
  struct SyncIndex : Row<int> {};
  struct SyncType : Row<int> {};

  //Used briefly to build the constraint axis
  struct CenterToContactX : Row<float> {};
  struct CenterToContactY : Row<float> {};
};
struct ConstraintObjA {};
struct ConstraintObjB {};

//Data common to both objects in a constraint pair
struct ConstraintData {
  struct LinearAxisX : Row<float> {};
  struct LinearAxisY : Row<float> {};
  struct AngularAxisA : Row<float> {};
  struct AngularAxisB : Row<float> {};
  struct ConstraintMass : Row<float> {};
  struct LinearImpulseX : Row<float> {};
  struct LinearImpulseY : Row<float> {};
  struct AngularImpulseA : Row<float> {};
  struct AngularImpulseB : Row<float> {};
  struct Bias : Row<float> {};
  struct LambdaSum : Row<float> {};

  //Temporarily used while building constraint list
  struct VisitData {
    enum class Location : uint8_t {
      InA,
      InB
    };
    bool operator<(size_t r) const {
      return mObjectIndex < r;
    }

    size_t mObjectIndex;
    size_t mConstraintIndex;
    Location mLocation;
  };
  using SharedVisitData = SharedRow<std::vector<VisitData>>;
};

//These are used to migrate final solved velocities constraint table back to the gameobjects
struct FinalSyncIndices {
  struct Mapping {
    size_t mSourceGamebject{};
    size_t mTargetConstraint{};
  };
  std::vector<Mapping> mMappingsA;
  std::vector<Mapping> mMappingsB;
};

using ConstraintsTable = Table<
  CollisionPairIndexA,
  CollisionPairIndexB,

  //Pretty clunky that this is needed but since order doesn't match between this table and narrowphase
  //it's easier to copy the data into the constraints table
  ContactPoint<ContactOne>::Overlap,
  ContactPoint<ContactTwo>::Overlap,
  SharedNormal::X,
  SharedNormal::Y,

  ConstraintObject<ConstraintObjA>::LinVelX,
  ConstraintObject<ConstraintObjA>::LinVelY,
  ConstraintObject<ConstraintObjA>::AngVel,
  ConstraintObject<ConstraintObjA>::SyncIndex,
  ConstraintObject<ConstraintObjA>::SyncType,
  ConstraintObject<ConstraintObjA>::CenterToContactX,
  ConstraintObject<ConstraintObjA>::CenterToContactY,

  ConstraintObject<ConstraintObjB>::LinVelX,
  ConstraintObject<ConstraintObjB>::LinVelY,
  ConstraintObject<ConstraintObjB>::AngVel,
  ConstraintObject<ConstraintObjB>::SyncIndex,
  ConstraintObject<ConstraintObjB>::SyncType,
  ConstraintObject<ConstraintObjB>::CenterToContactX,
  ConstraintObject<ConstraintObjB>::CenterToContactY,

  ConstraintData::LinearAxisX,
  ConstraintData::LinearAxisY,
  ConstraintData::AngularAxisA,
  ConstraintData::AngularAxisB,
  ConstraintData::ConstraintMass,
  ConstraintData::LinearImpulseX,
  ConstraintData::LinearImpulseY,
  ConstraintData::AngularImpulseA,
  ConstraintData::AngularImpulseB,
  ConstraintData::Bias,
  ConstraintData::LambdaSum,
  SharedRow<FinalSyncIndices>,
  ConstraintData::SharedVisitData
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

    template<class SrcRow, class DstRow, class DatabaseT>
    static void storeToRow(ConstraintsTable& table, DatabaseT& db, const std::vector<FinalSyncIndices::Mapping>& mappings) {
      SrcRow& src = std::get<SrcRow>(table.mRows);
      DstRow* dst = nullptr;
      DatabaseT::ElementID last;
      for(const FinalSyncIndices::Mapping mapping : mappings) {
        const DatabaseT::ElementID id(mapping.mSourceGamebject);
        if(!dst || last.getTableIndex() != id.getTableIndex()) {
          dst = Queries::getRowInTable<DstRow>(db, id);
        }

        if(dst) {
          dst->at(id.getElementIndex()) = src.at(mapping.mTargetConstraint);
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

  static void generateContacts(CollisionPairsTable& pairs);

  //Add the entries in the constraints table and figure out sync indices
  static void buildConstraintsTable(CollisionPairsTable& pairs, ConstraintsTable& constraints);

  //Migrate velocity data from db to constraint table
  template<class LinVelX, class LinVelY, class AngVel, class DatabaseT>
  static void fillConstraintVelocities(ConstraintsTable& constraints, DatabaseT& db) {
    std::vector<size_t>& idsA = std::get<CollisionPairIndexA>(constraints.mRows).mElements;
    details::fillRow<LinVelX, ConstraintObject<ConstraintObjA>::LinVelX>(constraints, db, idsA);
    details::fillRow<LinVelY, ConstraintObject<ConstraintObjA>::LinVelY>(constraints, db, idsA);
    details::fillRow<AngVel, ConstraintObject<ConstraintObjA>::AngVel>(constraints, db, idsA);

    std::vector<size_t>& idsB = std::get<CollisionPairIndexB>(constraints.mRows).mElements;
    details::fillRow<LinVelX, ConstraintObject<ConstraintObjB>::LinVelX>(constraints, db, idsB);
    details::fillRow<LinVelY, ConstraintObject<ConstraintObjB>::LinVelY>(constraints, db, idsB);
    details::fillRow<AngVel, ConstraintObject<ConstraintObjB>::AngVel>(constraints, db, idsB);
  }

  //Migrate velocity data from constraint table to db
  template<class LinVelX, class LinVelY, class AngVel, class DatabaseT>
  static void storeConstraintVelocities(ConstraintsTable& constraints, DatabaseT& db) {
    const FinalSyncIndices& indices = std::get<SharedRow<FinalSyncIndices>>(constraints.mRows).at();
    details::storeToRow<ConstraintObject<ConstraintObjA>::LinVelX, LinVelX>(constraints, db, indices.mMappingsA);
    details::storeToRow<ConstraintObject<ConstraintObjA>::LinVelY, LinVelY>(constraints, db, indices.mMappingsA);
    details::storeToRow<ConstraintObject<ConstraintObjA>::AngVel, AngVel>(constraints, db, indices.mMappingsA);

    details::storeToRow<ConstraintObject<ConstraintObjB>::LinVelX, LinVelX>(constraints, db, indices.mMappingsB);
    details::storeToRow<ConstraintObject<ConstraintObjB>::LinVelY, LinVelY>(constraints, db, indices.mMappingsB);
    details::storeToRow<ConstraintObject<ConstraintObjB>::AngVel, AngVel>(constraints, db, indices.mMappingsB);
  }

  static void setupConstraints(ConstraintsTable& constraints);
  static void solveConstraints(ConstraintsTable& constraints);
  static void storeWarmStarts(ConstraintsTable& constraints, CollisionPairsTable& pairs);
};