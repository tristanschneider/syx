#include "Precompile.h"
#include "Physics.h"

#include "glm/common.hpp"
#include "out_ispc/unity.h"
#include "PhysicsTableIds.h"
#include "TableOperations.h"

#include "glm/detail/func_geometric.inl"
#include "NotIspc.h"
#include "Profile.h"

namespace {
  template<class RowT>
  RowT* _unwrapRow(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table) {
    QueryResult<RowT> result = task.query<RowT>(table);
    return result.size() ? &result.get<0>(0) : nullptr;
  }

  struct UniformContactConstraintPairDataRows {
    Row<float>* linearAxisX;
    Row<float>* linearAxisY;
    Row<float>* angularAxisOneA;
    Row<float>* angularAxisOneB;
    Row<float>* angularAxisTwoA;
    Row<float>* angularAxisTwoB;
    Row<float>* angularFrictionAxisOneA;
    Row<float>* angularFrictionAxisOneB;
    Row<float>* angularFrictionAxisTwoA;
    Row<float>* angularFrictionAxisTwoB;
    Row<float>* constraintMassOne;
    Row<float>* constraintMassTwo;
    Row<float>* frictionConstraintMassOne;
    Row<float>* frictionConstraintMassTwo;
    Row<float>* linearImpulseX;
    Row<float>* linearImpulseY;
    Row<float>* angularImpulseOneA;
    Row<float>* angularImpulseOneB;
    Row<float>* angularImpulseTwoA;
    Row<float>* angularImpulseTwoB;
    Row<float>* angularFrictionImpulseOneA;
    Row<float>* angularFrictionImpulseOneB;
    Row<float>* angularFrictionImpulseTwoA;
    Row<float>* angularFrictionImpulseTwoB;
    Row<float>* biasOne;
    Row<float>* biasTwo;
  };

  ispc::UniformContactConstraintPairData _unwrapUniformConstraintData(UniformContactConstraintPairDataRows& rows) {
    return {
      rows.linearAxisX->data(),
      rows.linearAxisY->data(),
      rows.angularAxisOneA->data(),
      rows.angularAxisOneB->data(),
      rows.angularAxisTwoA->data(),
      rows.angularAxisTwoB->data(),
      rows.angularFrictionAxisOneA->data(),
      rows.angularFrictionAxisOneB->data(),
      rows.angularFrictionAxisTwoA->data(),
      rows.angularFrictionAxisTwoB->data(),
      rows.constraintMassOne->data(),
      rows.constraintMassTwo->data(),
      rows.frictionConstraintMassOne->data(),
      rows.frictionConstraintMassTwo->data(),
      rows.linearImpulseX->data(),
      rows.linearImpulseY->data(),
      rows.angularImpulseOneA->data(),
      rows.angularImpulseOneB->data(),
      rows.angularImpulseTwoA->data(),
      rows.angularImpulseTwoB->data(),
      rows.angularFrictionImpulseOneA->data(),
      rows.angularFrictionImpulseOneB->data(),
      rows.angularFrictionImpulseTwoA->data(),
      rows.angularFrictionImpulseTwoB->data(),
      rows.biasOne->data(),
      rows.biasTwo->data()
    };
  }

  UniformContactConstraintPairDataRows _unwrapUniformConstraintDataRows(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table) {
    return {
      _unwrapRow<ConstraintData::LinearAxisX>(task, table),
      _unwrapRow<ConstraintData::LinearAxisY>(task, table),
      _unwrapRow<ConstraintData::AngularAxisOneA>(task, table),
      _unwrapRow<ConstraintData::AngularAxisOneB>(task, table),
      _unwrapRow<ConstraintData::AngularAxisTwoA>(task, table),
      _unwrapRow<ConstraintData::AngularAxisTwoB>(task, table),
      _unwrapRow<ConstraintData::AngularFrictionAxisOneA>(task, table),
      _unwrapRow<ConstraintData::AngularFrictionAxisOneB>(task, table),
      _unwrapRow<ConstraintData::AngularFrictionAxisTwoA>(task, table),
      _unwrapRow<ConstraintData::AngularFrictionAxisTwoB>(task, table),
      _unwrapRow<ConstraintData::ConstraintMassOne>(task, table),
      _unwrapRow<ConstraintData::ConstraintMassTwo>(task, table),
      _unwrapRow<ConstraintData::FrictionConstraintMassOne>(task, table),
      _unwrapRow<ConstraintData::FrictionConstraintMassTwo>(task, table),
      _unwrapRow<ConstraintData::LinearImpulseX>(task, table),
      _unwrapRow<ConstraintData::LinearImpulseY>(task, table),
      _unwrapRow<ConstraintData::AngularImpulseOneA>(task, table),
      _unwrapRow<ConstraintData::AngularImpulseOneB>(task, table),
      _unwrapRow<ConstraintData::AngularImpulseTwoA>(task, table),
      _unwrapRow<ConstraintData::AngularImpulseTwoB>(task, table),
      _unwrapRow<ConstraintData::FrictionAngularImpulseOneA>(task, table),
      _unwrapRow<ConstraintData::FrictionAngularImpulseOneB>(task, table),
      _unwrapRow<ConstraintData::FrictionAngularImpulseTwoA>(task, table),
      _unwrapRow<ConstraintData::FrictionAngularImpulseTwoB>(task, table),
      _unwrapRow<ConstraintData::BiasOne>(task, table),
      _unwrapRow<ConstraintData::BiasTwo>(task, table)
    };
  }

  struct UniformConstraintObjectRows {
    Row<float>* linVelX;
    Row<float>* linVelY;
    Row<float>* angVel;
    Row<int32_t>* syncIndex;
    Row<int32_t>* syncType;
  };

  ispc::UniformConstraintObject _unwrapUniformConstraintObject(UniformConstraintObjectRows& rows) {
    return {
      rows.linVelX->data(),
      rows.linVelY->data(),
      rows.angVel->data(),
      rows.syncIndex->data(),
      rows.syncType->data()
    };
  }

  template<class CObj>
  UniformConstraintObjectRows _unwrapUniformConstraintObjectRows(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table) {
    using ConstraintT = ConstraintObject<CObj>;
    return {
      _unwrapRow<ConstraintT::LinVelX>(task, table),
      _unwrapRow<ConstraintT::LinVelY>(task, table),
      _unwrapRow<ConstraintT::AngVel>(task, table),
      _unwrapRow<ConstraintT::SyncIndex>(task, table),
      _unwrapRow<ConstraintT::SyncType>(task, table)
    };
  }

  struct LambdaSums {
    Row<float>* lambdaSumOne;
    Row<float>* lambdaSumTwo;
    Row<float>* frictionLambdaSumOne;
    Row<float>* frictionLambdaSumTwo;
    ConstraintData::CommonTableStartIndex* startIndex;
  };

  LambdaSums _unwrapLambdaSums(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table) {
    return {
      _unwrapRow<ConstraintData::LambdaSumOne>(task, table),
      _unwrapRow<ConstraintData::LambdaSumTwo>(task, table),
      _unwrapRow<ConstraintData::FrictionLambdaSumOne>(task, table),
      _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(task, table),
      _unwrapRow<ConstraintData::CommonTableStartIndex>(task, table)
    };
  }

  struct ContactConstraintSetupObjectRows {
    Row<float>* centerToContactOneX{};
    Row<float>* centerToContactOneY{};
    Row<float>* centerToContactTwoX{};
    Row<float>* centerToContactTwoY{};
  };

  struct ContactConstraintSetupRows {
    SharedNormal::X* sharedNormalX;
    SharedNormal::Y* sharedNormalY;
    ContactPoint<ContactOne>::Overlap* overlapOne;
    ContactPoint<ContactTwo>::Overlap* overlapTwo;
    ContactConstraintSetupObjectRows a;
    ContactConstraintSetupObjectRows b;
  };

  template<class Obj>
  ContactConstraintSetupObjectRows unwrapContactConstraintSetupObjectRows(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table) {
    return {
      _unwrapRow<ConstraintObject<Obj>::CenterToContactOneX>(task, table),
      _unwrapRow<ConstraintObject<Obj>::CenterToContactOneY>(task, table),
      _unwrapRow<ConstraintObject<Obj>::CenterToContactTwoX>(task, table),
      _unwrapRow<ConstraintObject<Obj>::CenterToContactTwoY>(task, table)
    };
  }

  ContactConstraintSetupRows unwrapContactConstraintSetupRows(RuntimeDatabaseTaskBuilder& task, const UnpackedDatabaseElementID& table) {
    return {
      _unwrapRow<SharedNormal::X>(task, table),
      _unwrapRow<SharedNormal::Y>(task, table),
      _unwrapRow<ContactPoint<ContactOne>::Overlap>(task, table),
      _unwrapRow<ContactPoint<ContactTwo>::Overlap>(task, table),
      unwrapContactConstraintSetupObjectRows<ConstraintObjA>(task, table),
      unwrapContactConstraintSetupObjectRows<ConstraintObjB>(task, table)
    };
  }
}

std::shared_ptr<TaskNode> Physics::details::fillCollisionMasks(TableResolver<CollisionMaskRow> resolver, std::vector<StableElementID>& idsA, std::vector<StableElementID>& idsB, const DatabaseDescription& desc, const PhysicsTableIds& tables) {
  CollisionMaskRow* dst = resolver.tryGetRow<CollisionMaskRow>(UnpackedDatabaseElementID::fromDescription(tables.mNarrowphaseTable, desc));
  assert(dst);
  return TaskNode::create([resolver, &idsA, &idsB, desc, dst](...) mutable {
    PROFILE_SCOPE("physics", "fillCollisionMasks");
    CachedRow<CollisionMaskRow> srcA;
    CachedRow<CollisionMaskRow> srcB;
    for(size_t i = 0; i < idsA.size(); ++i) {
      if(idsA[i] == StableElementID::invalid() || idsB[i] == StableElementID::invalid()) {
        dst->at(i) = 0;
        continue;
      }

      //Caller should ensure the unstable indices have been resolved such that now the unstable index is up to date
      const UnpackedDatabaseElementID idA = idsA[i].toUnpacked(desc);
      const UnpackedDatabaseElementID idB = idsB[i].toUnpacked(desc);
      resolver.tryGetOrSwapRow(srcA, idA);
      resolver.tryGetOrSwapRow(srcB, idB);
      if(srcA && srcB) {
        dst->at(i) = CollisionMask::combineForCollisionTable(srcA->at(idA.getElementIndex()), srcB->at(idB.getElementIndex()));
      }
    }
  });
}

void Physics::details::_integratePositionAxis(const float* velocity, float* position, size_t count) {
  ispc::integratePosition(position, velocity, uint32_t(count));
}

void Physics::details::_integrateRotation(float* rotX, float* rotY, const float* velocity, size_t count) {
  ispc::integrateRotation(rotX, rotY, velocity, uint32_t(count));
}

void Physics::details::_applyDampingMultiplier(float* velocity, float amount, size_t count) {
  ispc::applyDampingMultiplier(velocity, amount, uint32_t(count));
}
/*
void Physics::generateContacts(ContactInfo& info) {
  ispc::UniformConstVec2 positionsA{ info.a.posX, info.a.posY };
  ispc::UniformRotation rotationsA{ info.a.rotX, info.a.rotY };
  ispc::UniformConstVec2 positionsB{ info.b.posX, info.b.posY };
  ispc::UniformRotation rotationsB{ info.b.rotX, info.b.rotY };
  ispc::UniformVec2 normals{ info.normal.x, info.normal.y };
  ispc::UniformContact contactsOne{ info.one.pointX, info.one.pointY, info.one.overlap };
  ispc::UniformContact contactsTwo{ info.two.pointX, info.two.pointY, info.two.overlap };

  ispc::generateUnitCubeCubeContacts(positionsA, rotationsA, positionsB, rotationsB, normals, contactsOne, contactsTwo, uint32_t(info.count));
}

void Physics::generateContacts(CollisionPairsTable& pairs) {
  ispc::UniformConstVec2 positionsA{ _unwrapRow<NarrowphaseData<PairA>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairA>::PosY>(pairs) };
  ispc::UniformRotation rotationsA{ _unwrapRow<NarrowphaseData<PairA>::CosAngle>(pairs), _unwrapRow<NarrowphaseData<PairA>::SinAngle>(pairs) };
  ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  ispc::UniformRotation rotationsB{ _unwrapRow<NarrowphaseData<PairB>::CosAngle>(pairs), _unwrapRow<NarrowphaseData<PairB>::SinAngle>(pairs) };
  ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };

  ispc::UniformContact contactsOne{
    _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  };
  ispc::UniformContact contactsTwo{
    _unwrapRow<ContactPoint<ContactTwo>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactTwo>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactTwo>::Overlap>(pairs)
  };
  ispc::generateUnitCubeCubeContacts(positionsA, rotationsA, positionsB, rotationsB, normals, contactsOne, contactsTwo, uint32_t(TableOperations::size(pairs)));
  //ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));

  //ispc::UniformConstVec2 positionsA{ _unwrapRow<NarrowphaseData<PairA>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairA>::PosY>(pairs) };
  //ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  //ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };
  //ispc::UniformContact contacts{
  //  _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
  //  _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
  //  _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  //};
  //ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));
}
*/
void clearRow(IAppBuilder& builder, const QueryAlias<Row<float>> rowAlias) {
  auto task = builder.createTask();
  task.setName("clear row");
  QueryResult<Row<float>> rows = task.queryAlias(rowAlias);
  task.setCallback([rows](AppTaskArgs&) mutable {
    rows.forEachRow([](Row<float>& row) {
      std::memset(row.mElements.data(), 0, sizeof(float)*row.size());
    });
  });
  builder.submitTask(std::move(task));
}

void Physics::setupConstraints(IAppBuilder& builder) {
  //Currently computing as square
  //const float pi = 3.14159265359f;
  //const float r = 0.5f;
  //const float density = 1.0f;
  //const float mass = pi*r*r;
  //const float inertia = pi*(r*r*r*r)*0.25f;
  constexpr static float w = 1.0f;
  constexpr static float h = 1.0f;
  constexpr static float mass = w*h;
  constexpr static float inertia = mass*(h*h + w*w)/12.0f;
  constexpr static float invMass = 1.0f/mass;
  constexpr static float invInertia = 1.0f/inertia;
  constexpr static float bias = 0.1f;
  auto result = std::make_shared<TaskNode>();

  //TODO: don't clear this here and use it for warm start
  using QA = QueryAlias<Row<float>>;
  clearRow(builder,  QA::create<ConstraintData::LambdaSumOne>());
  clearRow(builder,  QA::create<ConstraintData::LambdaSumTwo>());
  clearRow(builder,  QA::create<ConstraintData::FrictionLambdaSumOne>());
  clearRow(builder,  QA::create<ConstraintData::FrictionLambdaSumOne>());

  auto contactTables = builder.queryTables<SharedMassConstraintsTableTag>();
  auto staticContactTables = builder.queryTables<ZeroMassConstraintsTableTag>();
  //Currently assuming one for simplicity, ultimately will probably change completely
  assert(contactTables.size() == staticContactTables.size() == 1);

  {
    auto task = builder.createTask();
    task.setName("setup contact constraints");
    ContactConstraintSetupRows setupRows = unwrapContactConstraintSetupRows(task, contactTables.matchingTableIDs[0]);
    UniformContactConstraintPairDataRows dataRows = _unwrapUniformConstraintDataRows(task, contactTables.matchingTableIDs[0]);

    task.setCallback([setupRows, dataRows](AppTaskArgs&) mutable {
      ispc::UniformVec2 normal{ setupRows.sharedNormalX->data(), setupRows.sharedNormalY->data() };
      ispc::UniformVec2 aToContactOne{ setupRows.a.centerToContactOneX->data(), setupRows.a.centerToContactOneY->data() };
      ispc::UniformVec2 bToContactOne{ setupRows.b.centerToContactOneX->data(), setupRows.b.centerToContactOneY->data() };
      ispc::UniformVec2 aToContactTwo{ setupRows.a.centerToContactTwoX->data(), setupRows.a.centerToContactTwoY->data() };
      ispc::UniformVec2 bToContactTwo{ setupRows.b.centerToContactTwoX->data(), setupRows.b.centerToContactTwoY->data() };
      float* overlapOne = setupRows.overlapOne->data();
      float* overlapTwo = setupRows.overlapTwo->data();
      ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(dataRows);
      const size_t count = setupRows.sharedNormalX->size();

      ispc::setupConstraintsSharedMass(invMass, invInertia, bias, normal, aToContactOne, aToContactTwo, bToContactOne, bToContactTwo, overlapOne, overlapTwo, data, uint32_t(count));
    });
    builder.submitTask(std::move(task));
  }

  {
    auto task = builder.createTask();
    task.setName("setup static contact constraints");
    ContactConstraintSetupRows setupRows = unwrapContactConstraintSetupRows(task, staticContactTables.matchingTableIDs[0]);
    UniformContactConstraintPairDataRows dataRows = _unwrapUniformConstraintDataRows(task, staticContactTables.matchingTableIDs[0]);

    task.setCallback([setupRows, dataRows](AppTaskArgs&) mutable {
      ispc::UniformVec2 normal = { setupRows.sharedNormalX->data(), setupRows.sharedNormalY->data() };
      ispc::UniformVec2 aToContactOne = { setupRows.a.centerToContactOneX->data(), setupRows.a.centerToContactOneY->data() };
      ispc::UniformVec2 aToContactTwo = { setupRows.a.centerToContactTwoX->data(), setupRows.a.centerToContactTwoY->data() };
      float* overlapOne = setupRows.overlapOne->data();
      float* overlapTwo = setupRows.overlapTwo->data();
      ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(dataRows);
      const size_t count = setupRows.sharedNormalX->size();

      ispc::setupConstraintsSharedMassBZeroMass(invMass, invInertia, bias, normal, aToContactOne, aToContactTwo, overlapOne, overlapTwo, data, uint32_t(count));
    });
    builder.submitTask(std::move(task));
  }
}

void Physics::solveConstraints(IAppBuilder& builder, const Config::PhysicsConfig& config) {
  //Everything in one since all velocities might depend on the previous ones. Can be more parallel with islands
  auto task = builder.createTask();
  task.setName("solve constraints");
  auto contactTables = builder.queryTables<SharedMassConstraintsTableTag>();
  auto staticContactTables = builder.queryTables<ZeroMassConstraintsTableTag>();
  auto commonTable = builder.queryTables<ConstraintsCommonTableTag>();
  //Currently assuming one for simplicity, ultimately will probably change completely
  assert(contactTables.size() == staticContactTables.size() == commonTable.size() == 1);
  UniformContactConstraintPairDataRows contactDataRows = _unwrapUniformConstraintDataRows(task, contactTables.matchingTableIDs[0]);
  UniformContactConstraintPairDataRows staticContactDataRows = _unwrapUniformConstraintDataRows(task, staticContactTables.matchingTableIDs[0]);
  UniformConstraintObjectRows objectARows = _unwrapUniformConstraintObjectRows<ConstraintObjA>(task, commonTable.matchingTableIDs[0]);
  UniformConstraintObjectRows objectBRows = _unwrapUniformConstraintObjectRows<ConstraintObjB>(task, commonTable.matchingTableIDs[0]);
  LambdaSums contactSums = _unwrapLambdaSums(task, contactTables.matchingTableIDs[0]);
  LambdaSums staticContactSums = _unwrapLambdaSums(task, staticContactTables.matchingTableIDs[0]);
  ConstraintData::IsEnabled* isEnabled = &task.query<ConstraintData::IsEnabled>(commonTable.matchingTableIDs[0]).get<0>(0);

  task.setCallback([=, &config](AppTaskArgs&) mutable {
    PROFILE_SCOPE("physics", "solve constraints");
    ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(contactDataRows);
    ispc::UniformConstraintObject objectA = _unwrapUniformConstraintObject(objectARows);
    ispc::UniformConstraintObject objectB = _unwrapUniformConstraintObject(objectBRows);
    float* lambdaSumOne = contactSums.lambdaSumOne->data();
    float* lambdaSumTwo = contactSums.lambdaSumTwo->data();
    float* frictionLambdaSumOne = contactSums.frictionLambdaSumOne->data();
    float* frictionLambdaSumTwo = contactSums.frictionLambdaSumTwo->data();
    uint8_t* enabled = isEnabled->data();

    const float frictionCoeff = config.frictionCoeff;
    const size_t startContact = contactSums.startIndex->at();
    const size_t startStatic = staticContactSums.startIndex->at();

    const bool oneAtATime = config.mForcedTargetWidth && *config.mForcedTargetWidth < ispc::getTargetWidth();

    {
      PROFILE_SCOPE("physics", "solveshared");
      const size_t count = contactSums.frictionLambdaSumOne->size();
      if(oneAtATime) {
        for(size_t i = 0; i < count; ++i) {
          ispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startContact), uint32_t(i), uint32_t(1));
        }
      }
      else {
        ispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startContact), uint32_t(0), uint32_t(count));
      }
    }

    data = _unwrapUniformConstraintData(staticContactDataRows);
    lambdaSumOne = staticContactSums.lambdaSumOne->data();
    lambdaSumTwo = staticContactSums.lambdaSumTwo->data();
    frictionLambdaSumOne = staticContactSums.frictionLambdaSumOne->data();
    frictionLambdaSumTwo = staticContactSums.frictionLambdaSumTwo->data();

    {
      PROFILE_SCOPE("physics", "solvezero");
      const size_t count = staticContactSums.frictionLambdaSumOne->size();
      if(oneAtATime) {
        for(size_t i = 0; i < count; ++i) {
          ispc::solveContactConstraintsBZeroMass(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startStatic), uint32_t(i), uint32_t(1));
        }
      }
      else {
        ispc::solveContactConstraintsBZeroMass(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startStatic), uint32_t(0), uint32_t(count));
      }
    }
  });

  builder.submitTask(std::move(task));
}

namespace PhysicsImpl {
  void applyDampingMultiplierAxis(IAppBuilder& builder, const QueryAlias<Row<float>>& axis, const float& multiplier) {
    for(const UnpackedDatabaseElementID& table : builder.queryAliasTables(axis).matchingTableIDs) {
      auto task = builder.createTask();
      task.setName("damping");
      Row<float>* axisRow = &task.queryAlias(table, axis).get<0>(0);
      task.setCallback([axisRow, &multiplier](AppTaskArgs&) {
        Physics::details::_applyDampingMultiplier(axisRow->mElements.data(), multiplier, axisRow->size());
      });

      builder.submitTask(std::move(task));
    }
  }

  void integratePositionAxis(IAppBuilder& builder, const QueryAlias<Row<float>>& position, const QueryAlias<Row<float>>& velocity) {
    for(const UnpackedDatabaseElementID& table : builder.queryAliasTables(position, velocity).matchingTableIDs) {
      auto task = builder.createTask();
      task.setName("Integrate Position");
      auto query = task.queryAlias(table, position, velocity.read());
      task.setCallback([query](AppTaskArgs&) mutable {
        Physics::details::_integratePositionAxis(
          query.get<1>(0).data(),
          query.get<0>(0).data(),
          query.get<0>(0).size()
        );
      });
      builder.submitTask(std::move(task));
    }
  }

  void storeToRow(IAppBuilder& builder, const QueryAlias<const Row<float>>& src, const QueryAlias<Row<float>>& dst, bool isA) {
    auto task = builder.createTask();
    std::shared_ptr<ITableResolver> dstResolver = task.getAliasResolver(dst);
    std::shared_ptr<IIDResolver> ids = task.getIDResolver();
    auto srcQuery = task.queryAlias(src, QueryAlias<SharedRow<FinalSyncIndices>>::create().read());
    task.setName("store constraint result");
    task.setCallback([srcQuery, dstResolver, isA, dst, ids](AppTaskArgs&) mutable {
      for(size_t i = 0; i < srcQuery.size(); ++i) {
        auto rows = srcQuery.get(i);
        const Row<float>* srcRow = std::get<const Row<float>*>(rows);
        const FinalSyncIndices& indices = std::get<1>(rows)->at();
        const auto& mappings = isA ? indices.mMappingsA : indices.mMappingsB;
        CachedRow<Row<float>> dstRow;

        for(const FinalSyncIndices::Mapping& mapping : mappings) {
          const UnpackedDatabaseElementID id = ids->uncheckedUnpack(mapping.mSourceGamebject);
          if(dstResolver->tryGetOrSwapRowAlias(dst, dstRow, id)) {
            dstRow->at(id.getElementIndex()) = srcRow->at(mapping.mTargetConstraint);
          }
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  template<bool(*enabledFn)(uint8_t)>
  void fillRow(IAppBuilder& builder,
    const QueryAlias<const Row<float>>& src,
    const QueryAlias<Row<float>>& dst,
    const QueryAlias<const Row<StableElementID>>& collisionPairIndices) {
    auto task = builder.createTask();
    task.setName("fillrow");
    auto enabledAlias = QueryAlias<const ConstraintData::IsEnabled>::create().read();
    std::shared_ptr<ITableResolver> srcResolver = task.getAliasResolver(src.read(), enabledAlias);
    std::shared_ptr<IIDResolver> ids = task.getIDResolver();
    auto query = task.queryAlias(dst, collisionPairIndices);

    task.setCallback([srcResolver, query, enabledAlias, src, ids](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        Row<float>& dstRow = query.get<0>(t);
        const Row<StableElementID>& pairIndicesRow = query.get<1>(t);
        const ConstraintData::IsEnabled* enabledRow = srcResolver->tryGetRowAlias(enabledAlias, query.matchingTableIDs[t]);
        CachedRow<const Row<float>> srcRow;
        for(size_t i = 0; i < dstRow.size(); ++i) {
          if(pairIndicesRow.at(i) == StableElementID::invalid() || (enabledRow && !enabledFn(enabledRow->at(i)))) {
            continue;
          }

          //Caller should ensure the unstable indices have been resolved such that now the unstable index is up to date
          const UnpackedDatabaseElementID id = ids->uncheckedUnpack(pairIndicesRow.at(i));
          if(srcResolver->tryGetOrSwapRowAlias(src.read(), srcRow, id)) {
            dstRow.at(i) = srcRow->at(id.getElementIndex());
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void fillConstraintRow(IAppBuilder& builder,
    const QueryAlias<const Row<float>>& src,
    const QueryAlias<Row<float>>& dst,
    const QueryAlias<const Row<StableElementID>>& collisionPairIndices) {
    fillRow<&CollisionMask::shouldSolveConstraint>(builder, src, dst, collisionPairIndices);
  }

  void fillNarrowphaseRow(IAppBuilder& builder,
    const QueryAlias<const Row<float>>& src,
    const QueryAlias<Row<float>>& dst,
    const QueryAlias<const Row<StableElementID>>& collisionPairIndices) {
    fillRow<&CollisionMask::shouldTestCollision>(builder, src, dst, collisionPairIndices);
  }

  //TODO: Should probably generalize to query the desired table properties rather than using singleton hardcoded ids
  PhysicsTableIds getTableIds(IAppBuilder& builder) {
    PhysicsTableIds ids;
    const UnpackedDatabaseElementID& someID = builder.queryTables<>().matchingTableIDs.front();
    ids.mTableIDMask = someID.getTableMask();
    ids.mElementIDMask = someID.getElementMask();
    ids.mSharedMassConstraintTable = builder.queryTables<SharedMassConstraintsTableTag>().matchingTableIDs.front().mValue;
    ids.mZeroMassConstraintTable = builder.queryTables<ZeroMassConstraintsTableTag>().matchingTableIDs.front().mValue;
    ids.mSharedMassObjectTable = builder.queryTables<SharedMassObjectTableTag>().matchingTableIDs.front().mValue;
    ids.mZeroMassObjectTable = builder.queryTables<ZeroMassObjectTableTag>().matchingTableIDs.front().mValue;
    ids.mConstriantsCommonTable = builder.queryTables<ConstraintsCommonTableTag>().matchingTableIDs.front().mValue;
    ids.mSpatialQueriesTable = builder.queryTables<SpatialQueriesTableTag>().matchingTableIDs.front().mValue;
    ids.mNarrowphaseTable = builder.queryTables<NarrowphaseTableTag>().matchingTableIDs.front().mValue;
    return ids;
  }

  void resolveCollisionTableIds(IAppBuilder& builder) {
    auto task = builder.createTask();
    std::shared_ptr<IIDResolver> ids = task.getIDResolver();
    auto query = task.query<CollisionPairIndexA, CollisionPairIndexB>();
    task.setName("collision id resolution");
    const PhysicsTableIds tableIDs = getTableIds(builder);
    task.setCallback([query, ids, tableIDs](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        CollisionPairIndexA& rowA = query.get<0>(t);
        CollisionPairIndexB& rowB = query.get<1>(t);
        for(size_t i = 0; i < rowA.size(); ++i) {
          StableElementID& a = rowA.at(i);
          StableElementID& b = rowB.at(i);
          std::optional<StableElementID> newA = ids->tryResolveStableID(a);
          std::optional<StableElementID> newB = ids->tryResolveStableID(b);
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
            if(!CollisionPairOrder::tryOrderCollisionPair(a, b, tableIDs)) {
              a = b = StableElementID::invalid();
            }
          }
        }
      }
    });
    builder.submitTask(std::move(task));
  }
}

void Physics::fillNarrowphaseData(IAppBuilder& builder, const PhysicsAliases& aliases) {
  //Id resolution must complete before the fill bundle starts
  PhysicsImpl::resolveCollisionTableIds(builder);

  using Alias = QueryAlias<Row<float>>;
  using OA = NarrowphaseData<PairA>;
  auto idsA = QueryAlias<Row<StableElementID>>::create<CollisionPairIndexA>().read();
  PhysicsImpl::fillNarrowphaseRow(builder, aliases.posX.read(), Alias::create<OA::PosX>(), idsA);
  PhysicsImpl::fillNarrowphaseRow(builder, aliases.posY.read(), Alias::create<OA::PosY>(), idsA);
  PhysicsImpl::fillNarrowphaseRow(builder, aliases.rotX.read(), Alias::create<OA::CosAngle>(), idsA);
  PhysicsImpl::fillNarrowphaseRow(builder, aliases.rotY.read(), Alias::create<OA::SinAngle>(), idsA);

  using OB = NarrowphaseData<PairB>;
  auto idsB = QueryAlias<Row<StableElementID>>::create<CollisionPairIndexB>().read();
  PhysicsImpl::fillNarrowphaseRow(builder, aliases.posX.read(), Alias::create<OB::PosX>(), idsB);
  PhysicsImpl::fillNarrowphaseRow(builder, aliases.posY.read(), Alias::create<OB::PosY>(), idsB);
  PhysicsImpl::fillNarrowphaseRow(builder, aliases.rotX.read(), Alias::create<OB::CosAngle>(), idsB);
  PhysicsImpl::fillNarrowphaseRow(builder, aliases.rotY.read(), Alias::create<OB::SinAngle>(), idsB);
}

void Physics::fillConstraintVelocities(IAppBuilder& builder, const PhysicsAliases& aliases) {
  using Alias = QueryAlias<Row<float>>;
  using OA = ConstraintObject<ConstraintObjA>;
  auto idsA = QueryAlias<Row<StableElementID>>::create<CollisionPairIndexA>().read();
  PhysicsImpl::fillConstraintRow(builder, aliases.linVelX.read(), Alias::create<OA::LinVelX>(), idsA);
  PhysicsImpl::fillConstraintRow(builder, aliases.linVelY.read(), Alias::create<OA::LinVelY>(), idsA);
  PhysicsImpl::fillConstraintRow(builder, aliases.angVel.read(), Alias::create<OA::AngVel>(), idsA);

  using OB = ConstraintObject<ConstraintObjA>;
  auto idsB = QueryAlias<Row<StableElementID>>::create<CollisionPairIndexB>().read();
  PhysicsImpl::fillConstraintRow(builder, aliases.linVelX.read(), Alias::create<OB::LinVelX>(), idsB);
  PhysicsImpl::fillConstraintRow(builder, aliases.linVelY.read(), Alias::create<OB::LinVelY>(), idsB);
  PhysicsImpl::fillConstraintRow(builder, aliases.angVel.read(), Alias::create<OB::AngVel>(), idsB);
}

void Physics::storeConstraintVelocities(IAppBuilder& builder, const PhysicsAliases& aliases) {
  using Alias = QueryAlias<Row<float>>;
  using OA = ConstraintObject<ConstraintObjA>;
  PhysicsImpl::storeToRow(builder, Alias::create<OA::LinVelX>().read(), aliases.linVelX, true);
  PhysicsImpl::storeToRow(builder, Alias::create<OA::LinVelY>().read(), aliases.linVelY, true);
  PhysicsImpl::storeToRow(builder, Alias::create<OA::AngVel>().read(), aliases.angVel, true);

  using OB = ConstraintObject<ConstraintObjB>;
  PhysicsImpl::storeToRow(builder, Alias::create<OB::LinVelX>().read(), aliases.linVelX, false);
  PhysicsImpl::storeToRow(builder, Alias::create<OB::LinVelY>().read(), aliases.linVelY, false);
  PhysicsImpl::storeToRow(builder, Alias::create<OB::AngVel>().read(), aliases.angVel, false);
}

void Physics::integratePosition(IAppBuilder& builder, const PhysicsAliases& aliases) {
  PhysicsImpl::integratePositionAxis(builder, aliases.posX, aliases.linVelX);
  PhysicsImpl::integratePositionAxis(builder, aliases.posY, aliases.linVelY);
}

void Physics::integrateRotation(IAppBuilder& builder, const PhysicsAliases& aliases) {
  for(const UnpackedDatabaseElementID& table : builder.queryAliasTables(aliases.rotX, aliases.rotY, aliases.angVel.read()).matchingTableIDs) {
    auto task = builder.createTask();
    auto query = task.queryAlias(table, aliases.rotX, aliases.rotY, aliases.angVel.read());
    task.setName("integrate rotation");
    task.setCallback([query](AppTaskArgs&) mutable {
      Physics::details::_integrateRotation(
        query.get<0>(0).mElements.data(),
        query.get<1>(0).mElements.data(),
        query.get<2>(0).mElements.data(),
        query.get<0>(0).size());
    });
    builder.submitTask(std::move(task));
  }
}

void Physics::applyDampingMultiplier(IAppBuilder& builder, const PhysicsAliases& aliases, const float& linearMultiplier, const float& angularMultiplier) {
  PhysicsImpl::applyDampingMultiplierAxis(builder, aliases.linVelX, linearMultiplier);
  PhysicsImpl::applyDampingMultiplierAxis(builder, aliases.linVelY, linearMultiplier);
  PhysicsImpl::applyDampingMultiplierAxis(builder, aliases.angVel, angularMultiplier);
}
