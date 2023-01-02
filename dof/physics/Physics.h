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

struct PhysicsTableIds {
  size_t mZeroMassTable{};
  size_t mSharedMassTable{};
  size_t mTableIDMask{};
};

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
  struct CenterToContactOneX : Row<float> {};
  struct CenterToContactOneY : Row<float> {};
  struct CenterToContactTwoX : Row<float> {};
  struct CenterToContactTwoY : Row<float> {};
};
struct ConstraintObjA {};
struct ConstraintObjB {};

//Data common to both objects in a constraint pair
struct ConstraintData {
  struct LinearAxisX : Row<float> {};
  struct LinearAxisY : Row<float> {};
  struct AngularAxisOneA : Row<float> {};
  struct AngularAxisOneB : Row<float> {};
  struct AngularAxisTwoA : Row<float> {};
  struct AngularAxisTwoB : Row<float> {};
  struct AngularFrictionAxisOneA : Row<float> {};
  struct AngularFrictionAxisOneB : Row<float> {};
  struct AngularFrictionAxisTwoA : Row<float> {};
  struct AngularFrictionAxisTwoB : Row<float> {};
  struct ConstraintMassOne : Row<float> {};
  struct ConstraintMassTwo : Row<float> {};
  struct FrictionConstraintMassOne : Row<float> {};
  struct FrictionConstraintMassTwo : Row<float> {};
  struct LinearImpulseX : Row<float> {};
  struct LinearImpulseY : Row<float> {};
  struct AngularImpulseOneA : Row<float> {};
  struct AngularImpulseOneB : Row<float> {};
  struct AngularImpulseTwoA : Row<float> {};
  struct AngularImpulseTwoB : Row<float> {};
  struct FrictionAngularImpulseOneA : Row<float> {};
  struct FrictionAngularImpulseOneB : Row<float> {};
  struct FrictionAngularImpulseTwoA : Row<float> {};
  struct FrictionAngularImpulseTwoB : Row<float> {};
  struct BiasOne : Row<float> {};
  struct BiasTwo : Row<float> {};
  struct LambdaSumOne : Row<float> {};
  struct LambdaSumTwo : Row<float> {};
  struct FrictionLambdaSumOne : Row<float> {};
  struct FrictionLambdaSumTwo : Row<float> {};

  //Temporarily used while building constraint list
  struct VisitData {
    enum class Location : uint8_t {
      InA,
      InB
    };
    bool operator<(size_t r) const {
      return mObjectIndex < r;
    }
    //Gameobject table entry that this is referring to
    size_t mObjectIndex{};
    //The latest constraint entry/location during population of constraints table that the object was found in
    size_t mConstraintIndex{};
    Location mLocation{};

    //The initial constraint entry/location, used to publish the final results back to the beginning for following iterations
    size_t mFirstConstraintIndex{};
    Location mFirstLocation{};
  };
  struct SharedVisitData {
    std::vector<VisitData> mVisited;
    std::deque<size_t> mIndicesToFill, mNextToFill;
  };
  using SharedVisitDataRow = SharedRow<SharedVisitData>;
  //Each constraint table has one of these which refers to the index into ConstraintCommonTable where the equivalent entries are stored.
  //The common table contains entries for each of the other tables in order, so only the start index is needed, and from there it's as if
  //the row is a part of the other table
  using CommonTableStartIndex = SharedRow<size_t>;
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

//There are several different tables for different constraint pair types,
//this is the common table that those methods use so that all object velocity copying happens within one table
using ConstraintCommonTable = Table<
  CollisionPairIndexA,
  CollisionPairIndexB,

  ConstraintObject<ConstraintObjA>::LinVelX,
  ConstraintObject<ConstraintObjA>::LinVelY,
  ConstraintObject<ConstraintObjA>::AngVel,
  ConstraintObject<ConstraintObjA>::SyncIndex,
  ConstraintObject<ConstraintObjA>::SyncType,

  ConstraintObject<ConstraintObjB>::LinVelX,
  ConstraintObject<ConstraintObjB>::LinVelY,
  ConstraintObject<ConstraintObjB>::AngVel,
  ConstraintObject<ConstraintObjB>::SyncIndex,
  ConstraintObject<ConstraintObjB>::SyncType,

  SharedRow<FinalSyncIndices>,
  ConstraintData::SharedVisitDataRow
>;

using ConstraintsTable = Table<
  //Pretty clunky that this is needed but since order doesn't match between this table and narrowphase
  //it's easier to copy the data into the constraints table
  ContactPoint<ContactOne>::Overlap,
  ContactPoint<ContactTwo>::Overlap,
  //Normal going towards A
  SharedNormal::X,
  SharedNormal::Y,

  ConstraintObject<ConstraintObjA>::CenterToContactOneX,
  ConstraintObject<ConstraintObjA>::CenterToContactOneY,
  ConstraintObject<ConstraintObjA>::CenterToContactTwoX,
  ConstraintObject<ConstraintObjA>::CenterToContactTwoY,

  ConstraintObject<ConstraintObjB>::CenterToContactOneX,
  ConstraintObject<ConstraintObjB>::CenterToContactOneY,
  ConstraintObject<ConstraintObjB>::CenterToContactTwoX,
  ConstraintObject<ConstraintObjB>::CenterToContactTwoY,

  ConstraintData::LinearAxisX,
  ConstraintData::LinearAxisY,
  ConstraintData::AngularAxisOneA,
  ConstraintData::AngularAxisTwoA,
  ConstraintData::AngularAxisOneB,
  ConstraintData::AngularAxisTwoB,
  ConstraintData::AngularFrictionAxisOneA,
  ConstraintData::AngularFrictionAxisTwoA,
  ConstraintData::AngularFrictionAxisOneB,
  ConstraintData::AngularFrictionAxisTwoB,
  ConstraintData::ConstraintMassOne,
  ConstraintData::ConstraintMassTwo,
  ConstraintData::FrictionConstraintMassOne,
  ConstraintData::FrictionConstraintMassTwo,
  ConstraintData::LinearImpulseX,
  ConstraintData::LinearImpulseY,
  ConstraintData::AngularImpulseOneA,
  ConstraintData::AngularImpulseOneB,
  ConstraintData::AngularImpulseTwoA,
  ConstraintData::AngularImpulseTwoB,
  ConstraintData::FrictionAngularImpulseOneA,
  ConstraintData::FrictionAngularImpulseOneB,
  ConstraintData::FrictionAngularImpulseTwoA,
  ConstraintData::FrictionAngularImpulseTwoB,
  ConstraintData::BiasOne,
  ConstraintData::BiasTwo,
  ConstraintData::LambdaSumOne,
  ConstraintData::LambdaSumTwo,
  ConstraintData::FrictionLambdaSumOne,
  ConstraintData::FrictionLambdaSumTwo,

  ConstraintData::CommonTableStartIndex
>;

using ContactConstraintsToStaticObjectsTable = Table<
  //Pretty clunky that this is needed but since order doesn't match between this table and narrowphase
  //it's easier to copy the data into the constraints table
  ContactPoint<ContactOne>::Overlap,
  ContactPoint<ContactTwo>::Overlap,
  //Normal going towards A
  SharedNormal::X,
  SharedNormal::Y,

  ConstraintObject<ConstraintObjA>::CenterToContactOneX,
  ConstraintObject<ConstraintObjA>::CenterToContactOneY,
  ConstraintObject<ConstraintObjA>::CenterToContactTwoX,
  ConstraintObject<ConstraintObjA>::CenterToContactTwoY,

  ConstraintData::LinearAxisX,
  ConstraintData::LinearAxisY,
  ConstraintData::AngularAxisOneA,
  ConstraintData::AngularAxisTwoA,
  ConstraintData::AngularFrictionAxisOneA,
  ConstraintData::AngularFrictionAxisTwoA,
  ConstraintData::ConstraintMassOne,
  ConstraintData::ConstraintMassTwo,
  ConstraintData::FrictionConstraintMassOne,
  ConstraintData::FrictionConstraintMassTwo,
  ConstraintData::LinearImpulseX,
  ConstraintData::LinearImpulseY,
  ConstraintData::AngularImpulseOneA,
  ConstraintData::AngularImpulseTwoA,
  ConstraintData::FrictionAngularImpulseOneA,
  ConstraintData::FrictionAngularImpulseTwoA,
  ConstraintData::BiasOne,
  ConstraintData::BiasTwo,
  ConstraintData::LambdaSumOne,
  ConstraintData::LambdaSumTwo,
  ConstraintData::FrictionLambdaSumOne,
  ConstraintData::FrictionLambdaSumTwo,
  SharedRow<FinalSyncIndices>,
  ConstraintData::SharedVisitDataRow,

  ConstraintData::CommonTableStartIndex
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

  static void generateCollisionPairs(const GridBroadphase::BroadphaseTable& broadphase, CollisionPairsTable& pairs, const PhysicsTableIds& tableIds);

  struct details {
    template<class SrcRow, class DstRow, class DatabaseT, class DstTableT>
    static void fillRow(DstTableT& table, DatabaseT& db, std::vector<size_t>& ids) {
      DstRow& dst = std::get<DstRow>(table.mRows);
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
    static void storeToRow(ConstraintCommonTable& table, DatabaseT& db, const std::vector<FinalSyncIndices::Mapping>& mappings) {
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

    static void _integratePositionAxis(float* velocity, float* position, size_t count);
    static void _integrateRotation(float* rotX, float* rotY, float* velocity, size_t count);
    static void _applyDampingMultiplier(float* velocity, float amount, size_t count);

    template<class Velocity, class Position, class DatabaseT>
    static void integratePositionAxis(DatabaseT& db) {
      Queries::viewEachRow<Velocity, Position>(db, [](Velocity& velocity, Position& position) {
        _integratePositionAxis(velocity.mElements.data(), position.mElements.data(), velocity.size());
      });
    }

    template<class Axis, class DatabaseT>
    static void applyDampingMultiplierAxis(DatabaseT& db, float multiplier) {
      Queries::viewEachRow<Axis>(db, [multiplier](Axis& axis) {
        _applyDampingMultiplier(axis.mElements.data(), multiplier, axis.size());
      });
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
  static void buildConstraintsTable(
    CollisionPairsTable& pairs,
    ConstraintsTable& constraints,
    ContactConstraintsToStaticObjectsTable& staticConstraints,
    ConstraintCommonTable& constraintsCommon,
    const PhysicsTableIds& tableIds);

  //Migrate velocity data from db to constraint table
  template<class LinVelX, class LinVelY, class AngVel, class DatabaseT>
  static void fillConstraintVelocities(ConstraintCommonTable& constraints, DatabaseT& db) {
    std::vector<size_t>& idsA = std::get<CollisionPairIndexA>(constraints.mRows).mElements;
    details::fillRow<LinVelX, ConstraintObject<ConstraintObjA>::LinVelX>(constraints, db, idsA);
    details::fillRow<LinVelY, ConstraintObject<ConstraintObjA>::LinVelY>(constraints, db, idsA);
    details::fillRow<AngVel, ConstraintObject<ConstraintObjA>::AngVel>(constraints, db, idsA);

    std::vector<size_t>& idsB = std::get<CollisionPairIndexB>(constraints.mRows).mElements;
    details::fillRow<LinVelX, ConstraintObject<ConstraintObjB>::LinVelX>(constraints, db, idsB);
    details::fillRow<LinVelY, ConstraintObject<ConstraintObjB>::LinVelY>(constraints, db, idsB);
    details::fillRow<AngVel, ConstraintObject<ConstraintObjB>::AngVel>(constraints, db, idsB);
  }

  static void setupConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts);
  static void solveConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts, ConstraintCommonTable& common);

  //Migrate velocity data from constraint table to db
  template<class LinVelX, class LinVelY, class AngVel, class DatabaseT>
  static void storeConstraintVelocities(ConstraintCommonTable& constraints, DatabaseT& db) {
    const FinalSyncIndices& indices = std::get<SharedRow<FinalSyncIndices>>(constraints.mRows).at();
    details::storeToRow<ConstraintObject<ConstraintObjA>::LinVelX, LinVelX>(constraints, db, indices.mMappingsA);
    details::storeToRow<ConstraintObject<ConstraintObjA>::LinVelY, LinVelY>(constraints, db, indices.mMappingsA);
    details::storeToRow<ConstraintObject<ConstraintObjA>::AngVel, AngVel>(constraints, db, indices.mMappingsA);

    details::storeToRow<ConstraintObject<ConstraintObjB>::LinVelX, LinVelX>(constraints, db, indices.mMappingsB);
    details::storeToRow<ConstraintObject<ConstraintObjB>::LinVelY, LinVelY>(constraints, db, indices.mMappingsB);
    details::storeToRow<ConstraintObject<ConstraintObjB>::AngVel, AngVel>(constraints, db, indices.mMappingsB);
  }

  template<class LinVelX, class LinVelY, class PosX, class PosY, class DatabaseT>
  static void integratePosition(DatabaseT& db) {
    details::integratePositionAxis<LinVelX, PosX>(db);
    details::integratePositionAxis<LinVelY, PosY>(db);
  }

  template<class CosAngle, class SinAngle, class AngVel, class DatabaseT>
  static void integrateRotation(DatabaseT& db) {
    Queries::viewEachRow<CosAngle, SinAngle, AngVel>(db, [](CosAngle& cosAngle, SinAngle& sinAngle, AngVel& angVel) {
      details::_integrateRotation(cosAngle.mElements.data(), sinAngle.mElements.data(), angVel.mElements.data(), cosAngle.size());
    });
  }

  template<class LinVelX, class LinVelY, class DatabaseT>
  static void applyDampingMultiplier(DatabaseT& db, float multiplier) {
    details::applyDampingMultiplierAxis<LinVelX>(db, multiplier);
    details::applyDampingMultiplierAxis<LinVelY>(db, multiplier);
  }
};