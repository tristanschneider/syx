#pragma once

#include "Profile.h"
#include "glm/vec2.hpp"
#include "config/Config.h"
#include "Queries.h"
#include "Scheduler.h"
#include "Table.h"
#include "NarrowphaseData.h"
#include "AppBuilder.h"

struct PhysicsTableIds;

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
  //Byte that incates the constraint should be solved 
  struct IsEnabled : Row<uint8_t> {};
  //Contact pair this constraint came from
  struct ConstraintContactPair : Row<StableElementID> {};

  //The destination in the common constraints table these should sync to.
  //They are the same as what SyncIndex will end up as but used during constraint table updates
  //to ensure those indices are pointing at the right elements
  struct PreSyncElements {
    StableElementID mASyncDest, mBSyncDest;
  };

  //Temporarily used while building constraint list
  struct VisitData {
    enum class Location : uint8_t {
      InA,
      InB
    };
    bool operator<(const StableElementID& r) const {
      return mObjectIndex.mStableID < r.mStableID;
    }
    //Gameobject table entry that this is referring to
    StableElementID mObjectIndex{};
    //The latest constraint entry/location during population of constraints table that the object was found in
    size_t mConstraintIndex{};
    Location mLocation{};

    //The initial constraint entry/location, used to publish the final results back to the beginning for following iterations
    size_t mFirstConstraintIndex{};
    Location mFirstLocation{};
  };
  struct SharedVisitData {
    std::unordered_map<size_t, VisitData> mVisited;
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
    StableElementID mSourceGamebject{};
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
  ConstraintData::ConstraintContactPair,

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

  ConstraintData::IsEnabled,

  StableIDRow,

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

  ConstraintData::CommonTableStartIndex
>;

struct PhysicsAliases {
  using FloatAlias = QueryAlias<Row<float>>;

  FloatAlias posX;
  FloatAlias posY;
  FloatAlias rotX;
  FloatAlias rotY;
  FloatAlias linVelX;
  FloatAlias linVelY;
  FloatAlias angVel;
};

struct ContactObjectStore {
  float* posX{};
  float* posY{};
  float* rotX{};
  float* rotY{};
};

struct ContactPointStore {
  float* pointX{};
  float* pointY{};
  float* overlap{};
};

struct ContactNormalStore {
  float* x{};
  float* y{};
};

struct ContactInfo {
  ContactObjectStore a, b;
  ContactPointStore one, two;
  ContactNormalStore normal;
  size_t count{};
};

struct Physics {
  struct details {
    template<bool(*enabledFn)(uint8_t), class SrcRow, class DstRow, class DatabaseT, class DstTableT>
    static std::shared_ptr<TaskNode> fillRow(DstTableT& table, DatabaseT& db, std::vector<StableElementID>& ids, const std::vector<uint8_t>* isEnabled = nullptr) {
      return TaskNode::create([&table, &db, &ids, isEnabled](...) {
        PROFILE_SCOPE("physics", "fillRow");
        DstRow& dst = std::get<DstRow>(table.mRows);
        SrcRow* src = nullptr;
        DatabaseT::ElementID last;
        for(size_t i = 0; i < ids.size(); ++i) {
          if(ids[i] == StableElementID::invalid() || (isEnabled && !enabledFn(isEnabled->at(i)))) {
            continue;
          }

          //Caller should ensure the unstable indices have been resolved such that now the unstable index is up to date
          const DatabaseT::ElementID id(ids[i].mUnstableIndex);
          //Retreive the rows every time the tables change, which should be rarely
          if(!src || last.getTableIndex() != id.getTableIndex()) {
            src = Queries::getRowInTable<SrcRow>(db, id);
          }

          if(src) {
            dst.at(i) = src->at(id.getElementIndex());
          }

          last = id;
        }
      });
    }

    template<class SrcRow, class DstRow, class DatabaseT, class DstTableT>
    static std::shared_ptr<TaskNode> fillNarrowphaseRow(DstTableT& table, DatabaseT& db, std::vector<StableElementID>& ids, const std::vector<uint8_t>* isEnabled = nullptr) {
      return fillRow<&CollisionMask::shouldTestCollision, SrcRow, DstRow>(table, db, ids, isEnabled);
    }

    static std::shared_ptr<TaskNode> fillCollisionMasks(TableResolver<CollisionMaskRow> resolver, std::vector<StableElementID>& idsA, std::vector<StableElementID>& idsB, const DatabaseDescription& desc, const PhysicsTableIds& tables);

    static void _integratePositionAxis(const float* velocity, float* position, size_t count);
    static void _integrateRotation(float* rotX, float* rotY, const float* velocity, size_t count);
    static void _applyDampingMultiplier(float* velocity, float amount, size_t count);
  };

  template<class DatabaseT>
  static void resolveCollisionTableIds(CollisionPairsTable& pairs, DatabaseT& db, StableElementMappings& mappings, const PhysicsTableIds& physicsTables) {
    std::vector<StableElementID>& idsA = std::get<CollisionPairIndexA>(pairs.mRows).mElements;
    std::vector<StableElementID>& idsB = std::get<CollisionPairIndexB>(pairs.mRows).mElements;
    for(size_t i = 0; i < idsA.size(); ++i) {
      auto& a = idsA[i];
      auto& b = idsB[i];
      std::optional<StableElementID> newA = StableOperations::tryResolveStableID(a, db, mappings);
      std::optional<StableElementID> newB = StableOperations::tryResolveStableID(b, db, mappings);
      //If an id disappeared, invalidate the pair
      if(!newA || !newB) {
        a = b = StableElementID::invalid();
      }
      //If the ids changes, reorder the collision pair
      else if(*newA != a || *newB != b) {
        //Write the newly resolved handles back to the stored ids
        a = *newA;
        b = *newB;
        //Reorder them in case the new element location caused a collision pair order change
        if(!CollisionPairOrder::tryOrderCollisionPair(a, b, physicsTables)) {
          a = b = StableElementID::invalid();
        }
      }
    }
  }

  //Populates narrowphase data by fetching it from the provided input using the indices stored by the broadphase
  template<class PosX, class PosY, class CosAngle, class SinAngle, class DatabaseT>
  static TaskRange fillNarrowphaseData(CollisionPairsTable& pairs, DatabaseT& db, StableElementMappings& mappings, const PhysicsTableIds& physicsTables) {
    //Id resolution must complete before the fill bundle starts
    auto root = TaskNode::create([&](...) {
      PROFILE_SCOPE("physics", "resolveCollisionTableIds");
      resolveCollisionTableIds(pairs, db, mappings, physicsTables);
    });
    std::vector<StableElementID>& idsA = std::get<CollisionPairIndexA>(pairs.mRows).mElements;
    std::vector<StableElementID>& idsB = std::get<CollisionPairIndexB>(pairs.mRows).mElements;

    root->mChildren.push_back(details::fillNarrowphaseRow<PosX, NarrowphaseData<PairA>::PosX>(pairs, db, idsA));
    root->mChildren.push_back(details::fillNarrowphaseRow<PosY, NarrowphaseData<PairA>::PosY>(pairs, db, idsA));
    root->mChildren.push_back(details::fillNarrowphaseRow<CosAngle, NarrowphaseData<PairA>::CosAngle>(pairs, db, idsA));
    root->mChildren.push_back(details::fillNarrowphaseRow<SinAngle, NarrowphaseData<PairA>::SinAngle>(pairs, db, idsA));

    root->mChildren.push_back(details::fillNarrowphaseRow<PosX, NarrowphaseData<PairB>::PosX>(pairs, db, idsB));
    root->mChildren.push_back(details::fillNarrowphaseRow<PosY, NarrowphaseData<PairB>::PosY>(pairs, db, idsB));
    root->mChildren.push_back(details::fillNarrowphaseRow<CosAngle, NarrowphaseData<PairB>::CosAngle>(pairs, db, idsB));
    root->mChildren.push_back(details::fillNarrowphaseRow<SinAngle, NarrowphaseData<PairB>::SinAngle>(pairs, db, idsB));

    return TaskBuilder::addEndSync(root);
  }

  static void generateContacts(CollisionPairsTable& pairs);
  static void generateContacts(ContactInfo& info);

  //Migrate velocity data from db to constraint table
  static void fillConstraintVelocities(IAppBuilder& builder, const PhysicsAliases& aliases);

  static TaskRange setupConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts);
  static TaskRange solveConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts, ConstraintCommonTable& common, const Config::PhysicsConfig& config);

  //Migrate velocity data from constraint table to db
  static void storeConstraintVelocities(IAppBuilder& builder, const PhysicsAliases& aliases);
  static void integratePosition(IAppBuilder& builder, const PhysicsAliases& aliases);
  static void integrateRotation(IAppBuilder& builder, const PhysicsAliases& aliases);
  static void applyDampingMultiplier(IAppBuilder& builder, const PhysicsAliases& aliases, const float& linearMultiplier, const float& angularMultiplier);
};