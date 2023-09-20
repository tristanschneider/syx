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
  template<class RowT, class TableT>
  decltype(std::declval<RowT&>().mElements.data()) _unwrapRow(TableT& t) {
    if constexpr(TableOperations::hasRow<RowT, TableT>()) {
      return std::get<RowT>(t.mRows).mElements.data();
    }
   else {
      return nullptr;
    }
  }

  template<class TableT>
  ispc::UniformContactConstraintPairData _unwrapUniformConstraintData(TableT& constraints) {
    return {
      _unwrapRow<ConstraintData::LinearAxisX>(constraints),
      _unwrapRow<ConstraintData::LinearAxisY>(constraints),
      _unwrapRow<ConstraintData::AngularAxisOneA>(constraints),
      _unwrapRow<ConstraintData::AngularAxisOneB>(constraints),
      _unwrapRow<ConstraintData::AngularAxisTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularAxisTwoB>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisOneA>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisOneB>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisTwoB>(constraints),
      _unwrapRow<ConstraintData::ConstraintMassOne>(constraints),
      _unwrapRow<ConstraintData::ConstraintMassTwo>(constraints),
      _unwrapRow<ConstraintData::FrictionConstraintMassOne>(constraints),
      _unwrapRow<ConstraintData::FrictionConstraintMassTwo>(constraints),
      _unwrapRow<ConstraintData::LinearImpulseX>(constraints),
      _unwrapRow<ConstraintData::LinearImpulseY>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseOneA>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseOneB>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseTwoB>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseOneA>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseOneB>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseTwoA>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseTwoB>(constraints),
      _unwrapRow<ConstraintData::BiasOne>(constraints),
      _unwrapRow<ConstraintData::BiasTwo>(constraints)
    };
  }

  template<class CObj>
  ispc::UniformConstraintObject _unwrapUniformConstraintObject(ConstraintCommonTable& constraints) {
    using ConstraintT = ConstraintObject<CObj>;
    return {
      _unwrapRow<ConstraintT::LinVelX>(constraints),
      _unwrapRow<ConstraintT::LinVelY>(constraints),
      _unwrapRow<ConstraintT::AngVel>(constraints),
      _unwrapRow<ConstraintT::SyncIndex>(constraints),
      _unwrapRow<ConstraintT::SyncType>(constraints)
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

std::shared_ptr<TaskNode> _clearRow(Row<float>& row) {
  return TaskNode::create([&row](...) {
    std::memset(row.mElements.data(), 0, sizeof(float)*row.size());
  });
}

TaskRange Physics::setupConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts) {
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
  result->mChildren.push_back(_clearRow(std::get<ConstraintData::LambdaSumOne>(constraints.mRows)));
  result->mChildren.push_back(_clearRow(std::get<ConstraintData::LambdaSumTwo>(constraints.mRows)));
  result->mChildren.push_back(_clearRow(std::get<ConstraintData::FrictionLambdaSumOne>(constraints.mRows)));
  result->mChildren.push_back(_clearRow(std::get<ConstraintData::FrictionLambdaSumTwo>(constraints.mRows)));

  result->mChildren.push_back(TaskNode::create([&constraints](...) {
    PROFILE_SCOPE("physics", "setupsharedmass");
    ispc::UniformVec2 normal{ _unwrapRow<SharedNormal::X>(constraints), _unwrapRow<SharedNormal::Y>(constraints) };
    ispc::UniformVec2 aToContactOne{ _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneY>(constraints) };
    ispc::UniformVec2 bToContactOne{ _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactOneX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactOneY>(constraints) };
    ispc::UniformVec2 aToContactTwo{ _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(constraints) };
    ispc::UniformVec2 bToContactTwo{ _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoY>(constraints) };
    float* overlapOne = _unwrapRow<ContactPoint<ContactOne>::Overlap>(constraints);
    float* overlapTwo = _unwrapRow<ContactPoint<ContactTwo>::Overlap>(constraints);
    ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(constraints);

    ispc::setupConstraintsSharedMass(invMass, invInertia, bias, normal, aToContactOne, aToContactTwo, bToContactOne, bToContactTwo, overlapOne, overlapTwo, data, uint32_t(TableOperations::size(constraints)));
  }));

  result->mChildren.push_back(_clearRow(std::get<ConstraintData::LambdaSumOne>(staticContacts.mRows)));
  result->mChildren.push_back(_clearRow(std::get<ConstraintData::LambdaSumTwo>(staticContacts.mRows)));
  result->mChildren.push_back(_clearRow(std::get<ConstraintData::FrictionLambdaSumOne>(staticContacts.mRows)));
  result->mChildren.push_back(_clearRow(std::get<ConstraintData::FrictionLambdaSumTwo>(staticContacts.mRows)));

  result->mChildren.push_back(TaskNode::create([&staticContacts](...) {
    PROFILE_SCOPE("physics", "setupzeromass");
    ispc::UniformVec2 normal = { _unwrapRow<SharedNormal::X>(staticContacts), _unwrapRow<SharedNormal::Y>(staticContacts) };
    ispc::UniformVec2 aToContactOne = { _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneX>(staticContacts), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneY>(staticContacts) };
    ispc::UniformVec2 aToContactTwo = { _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(staticContacts), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(staticContacts) };
    float* overlapOne = _unwrapRow<ContactPoint<ContactOne>::Overlap>(staticContacts);
    float* overlapTwo = _unwrapRow<ContactPoint<ContactTwo>::Overlap>(staticContacts);
    ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(staticContacts);

    ispc::setupConstraintsSharedMassBZeroMass(invMass, invInertia, bias, normal, aToContactOne, aToContactTwo, overlapOne, overlapTwo, data, uint32_t(TableOperations::size(staticContacts)));
  }));

  return TaskBuilder::addEndSync(result);
}

TaskRange Physics::solveConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts, ConstraintCommonTable& common, const Config::PhysicsConfig& config) {
  //Everything in one since all velocities might depend on the previous ones. Can be more parallel with islands
  auto result = TaskNode::create([&constraints, &staticContacts, &common, &config](...) {
    PROFILE_SCOPE("physics", "solve constraints");
    ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(constraints);
    ispc::UniformConstraintObject objectA = _unwrapUniformConstraintObject<ConstraintObjA>(common);
    ispc::UniformConstraintObject objectB = _unwrapUniformConstraintObject<ConstraintObjB>(common);
    float* lambdaSumOne = _unwrapRow<ConstraintData::LambdaSumOne>(constraints);
    float* lambdaSumTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(constraints);
    float* frictionLambdaSumOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(constraints);
    float* frictionLambdaSumTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(constraints);
    uint8_t* enabled = _unwrapRow<ConstraintData::IsEnabled>(common);

    const float frictionCoeff = config.frictionCoeff;
    const size_t startContact = std::get<ConstraintData::CommonTableStartIndex>(constraints.mRows).at();
    const size_t startStatic = std::get<ConstraintData::CommonTableStartIndex>(staticContacts.mRows).at();

    const bool oneAtATime = config.mForcedTargetWidth && *config.mForcedTargetWidth < ispc::getTargetWidth();

    {
      PROFILE_SCOPE("physics", "solveshared");
      if(oneAtATime) {
        for(size_t i = 0; i < TableOperations::size(constraints); ++i) {
          ispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startContact), uint32_t(i), uint32_t(1));
        }
      }
      else {
        ispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startContact), uint32_t(0), uint32_t(TableOperations::size(constraints)));
      }
    }

    data = _unwrapUniformConstraintData(staticContacts);
    lambdaSumOne = _unwrapRow<ConstraintData::LambdaSumOne>(staticContacts);
    lambdaSumTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(staticContacts);
    frictionLambdaSumOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(staticContacts);
    frictionLambdaSumTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(staticContacts);

    {
      PROFILE_SCOPE("physics", "solvezero");
      if(oneAtATime) {
        for(size_t i = 0; i < TableOperations::size(staticContacts); ++i) {
          ispc::solveContactConstraintsBZeroMass(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startStatic), uint32_t(i), uint32_t(1));
        }
      }
      else {
        ispc::solveContactConstraintsBZeroMass(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, enabled, frictionCoeff, uint32_t(startStatic), uint32_t(0), uint32_t(TableOperations::size(staticContacts)));
      }
    }
  });

  return { result, result };
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
