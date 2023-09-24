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

struct SharedMassConstraintsTableTag : SharedRow<char> {};
struct ZeroMassConstraintsTableTag : SharedRow<char> {};
struct SharedMassObjectTableTag : SharedRow<char> {};
struct ZeroMassObjectTableTag : SharedRow<char> {};
struct ConstraintsCommonTableTag : SharedRow<char> {};
struct SpatialQueriesTableTag : SharedRow<char> {};

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
  ConstraintsCommonTableTag,
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
  SharedMassConstraintsTableTag,
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
  ZeroMassConstraintsTableTag,
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

namespace Physics {
  namespace details {
    void _integratePositionAxis(const float* velocity, float* position, size_t count);
    void _integrateRotation(float* rotX, float* rotY, const float* velocity, size_t count);
    void _applyDampingMultiplier(float* velocity, float amount, size_t count);
  };

  //TODO: expose createDatabase here rather than having gameplay create the physics tables

  //Populates narrowphase data by fetching it from the provided input using the indices stored by the broadphase
  void updateNarrowphase(IAppBuilder& builder, const PhysicsAliases& aliases);
  //Migrate velocity data from db to constraint table
  void fillConstraintVelocities(IAppBuilder& builder, const PhysicsAliases& aliases);

  void setupConstraints(IAppBuilder& builder);
  void solveConstraints(IAppBuilder& builder, const Config::PhysicsConfig& config);

  //Migrate velocity data from constraint table to db
  void storeConstraintVelocities(IAppBuilder& builder, const PhysicsAliases& aliases);
  void integratePosition(IAppBuilder& builder, const PhysicsAliases& aliases);
  void integrateRotation(IAppBuilder& builder, const PhysicsAliases& aliases);
  void applyDampingMultiplier(IAppBuilder& builder, const PhysicsAliases& aliases, const float& linearMultiplier, const float& angularMultiplier);
};