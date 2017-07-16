#include "Precompile.h"
#include "SyxSpace.h"
#include "SyxAABBTree.h"

namespace Syx {
  typedef AABBTree DefaultBroadphase;
  typedef AABBTreeContext DefaultBroadphaseContext;

  Space::Space(void)
    : mBroadphase(new DefaultBroadphase())
    , mBroadphaseContext(new DefaultBroadphaseContext()) {
    mConstraintSystem.SetIslandGraph(mIslandGraph);
  }

  Space::Space(Handle handle)
    : mMyHandle(handle)
    , mBroadphase(new DefaultBroadphase())
    , mBroadphaseContext(new DefaultBroadphaseContext()) {
    mConstraintSystem.SetIslandGraph(mIslandGraph);
  }

  Space::Space(const Space& rhs) {
    *this = rhs;
    mObjects.Reserve(100);
    mBroadphase = new DefaultBroadphase();
  }

  Space::~Space(void) {
    delete mBroadphase;
    delete mBroadphaseContext;
  }

  Space& Space::operator=(const Space& rhs) {
    *mBroadphase = *rhs.mBroadphase;
    mConstraintSystem = rhs.mConstraintSystem;
    mMyHandle = rhs.mMyHandle;
    mNarrowphase = rhs.mNarrowphase;
    mObjects = rhs.mObjects;
    mProfiler = rhs.mProfiler;
    mConstraintSystem.SetIslandGraph(mIslandGraph);
    return *this;
  }

  PhysicsObject* Space::CreateObject(void) {
    return mObjects.Add();
  }

  void Space::DestroyObject(Handle handle) {
    PhysicsObject* obj = mObjects.Get(handle);
    while(!obj->GetConstraints().empty())
      RemoveConstraint(*obj->GetConstraints().begin());
    mIslandGraph.Remove(*mObjects.Get(handle));
    mObjects.Remove(handle);
  }

  PhysicsObject* Space::GetObject(Handle handle) {
    return mObjects.Get(handle);
  }

  void Space::Clear(void) {
    mObjects.Clear();
    mBroadphase->Clear();
    mConstraintSystem.Clear();
    mIslandGraph.Clear();
  }

  void Space::Update(float dt) {
    AutoProfileBlock block(mProfiler, "Update Space");

    IntegrateVelocity(dt);
    if((Interface::GetOptions().mDebugFlags & SyxOptions::DisableCollision) == 0)
      CollisionDetection();
    SolveConstraints(dt);
    IntegratePosition(dt);
  }

  void Space::IntegrateVelocity(float dt) {
    AutoProfileBlock block(mProfiler, "Integrate Velocity");
    if(Interface::GetOptions().mSimdFlags & SyxOptions::SIMD::VelocityIntegration)
      SIntegrateAllVelocity(dt);
    else
      IntegrateAllVelocity(dt);
  }

  void Space::IntegrateAllVelocity(float dt) {
    for(auto it = mObjects.Begin(); it != mObjects.End(); ++it) {
      PhysicsObject& obj = *it;
      if(obj.GetAsleep())
        continue;
      Rigidbody* rigidbody = obj.GetRigidbody();
      if(rigidbody)
        rigidbody->IntegrateVelocity(dt);
    }
  }

  void Space::UpdateMovedObject(PhysicsObject& obj) {
    obj.UpdateModelInst();
    Collider* collider = obj.GetCollider();
    if(collider)
      collider->mBroadHandle = mBroadphase->Update(BoundingVolume(collider->GetAABB()), collider->mBroadHandle);
  }

  void Space::WakeObject(PhysicsObject& obj) {
    mIslandGraph.WakeIsland(obj);
  }

  void Space::IntegratePosition(float dt) {
    {
      AutoProfileBlock block(mProfiler, "Integrate Position");
      if(Interface::GetOptions().mSimdFlags & SyxOptions::SIMD::PositionIntegration)
        SIntegrateAllPositions(dt);
      else
        IntegrateAllPositions(dt);
    }

    {
      AutoProfileBlock block(mProfiler, "Update Manifolds");
      mConstraintSystem.UpdateManifolds();
    }

    {
      DebugDrawer& d = DebugDrawer::Get();
      AutoProfileBlock draw(mProfiler, "Debug Draw");
      Vec3 color = Vec3::Zero;
      color[mMyHandle % 3] = 1.0f;
      if(Interface::GetOptions().mDebugFlags & (SyxOptions::Debug::DrawModels | SyxOptions::Debug::DrawPersonalBBs))
        for(auto it = mObjects.Begin(); it != mObjects.End(); ++it) {
          PhysicsObject& obj = *it;
          d.SetColor(color.x, color.y, color.z);
          obj.DrawModel();
          if((Interface::GetOptions().mDebugFlags & SyxOptions::Debug::DrawSleeping) && obj.GetAsleep()) {
            d.SetColor(1.0f, 1.0f, 1.0f);
            d.DrawPoint(obj.GetTransform().mPos, 0.1f);
          }
        }

      if(Interface::GetOptions().mDebugFlags & SyxOptions::Debug::DrawBroadphase)
        mBroadphase->Draw();

      if(Interface::GetOptions().mDebugFlags & SyxOptions::Debug::DrawIslands) {
        Vec3 colors[] = {Vec3::Zero
          , Vec3::UnitX
          , Vec3::UnitY
          , Vec3::UnitZ
          , Vec3::UnitX + Vec3::UnitY
          , Vec3::UnitX + Vec3::UnitZ
          , Vec3::UnitY + Vec3::UnitZ
          , Vec3::Identity};
        size_t colorCount = 8;

        for(size_t i = 0; i < mIslandGraph.IslandCount(); ++i) {
          Vec3 c = colors[i % colorCount];
          d.SetColor(c.x, c.y, c.z);
          mIslandGraph.GetIsland(i, mIslandStore, true);
          for(Constraint* constraint : mIslandStore.mConstraints) {
            //Don't draw static object lines as it clutters visuals quickly for ground planes
            if(!constraint->GetObjA()->IsStatic() && !constraint->GetObjB()->IsStatic())
              d.DrawLine(constraint->GetObjA()->GetTransform().mPos, constraint->GetObjB()->GetTransform().mPos);
          }
        }
      }
    }
  }

  void Space::IntegrateAllPositions(float dt) {
    for(auto it = mObjects.Begin(); it != mObjects.End(); ++it) {
      PhysicsObject& obj = *it;
      if(obj.GetAsleep())
        continue;
      Rigidbody* rigidbody = obj.GetRigidbody();
      if(rigidbody) {
        rigidbody->IntegratePosition(dt);
        UpdateMovedObject(*it);
      }
    }
  }

  void Space::CollisionDetection(void) {
    AutoProfileBlock detection(mProfiler, "Collision Detection");

    mProfiler.PushBlock("Broadphase");
    mBroadphase->QueryPairs(*mBroadphaseContext);
    mProfiler.PopBlock("Broadphase");

    {
      AutoProfileBlock narrow(mProfiler, "Narrowphase");
      mNarrowphase.ProcessPairQuery(mBroadphaseContext->mQueryPairResults, *this);
    }
  }

  void Space::SolveConstraints(float dt) {
    AutoProfileBlock block(mProfiler, "Constraint Solving");
    if(gOptions.mSimdFlags & SyxOptions::SIMD::ConstraintSolve)
      mConstraintSystem.SSolve(dt);
    else
      mConstraintSystem.Solve(dt);
  }

  CastResult Space::LineCastAll(const Vec3& start, const Vec3& end) {
    mBroadphase->QueryRaycast(start, end, *mBroadphaseContext);
    mCasterContext.ClearResults();

    for(const ResultNode& obj : mBroadphaseContext->mQueryResults) {
      PhysicsObject* pObj = reinterpret_cast<PhysicsObject*>(obj.mUserdata);
      mCaster.LineCast(*pObj, start, end, mCasterContext);
    }

    mCasterContext.SortResults();
    return CastResult(&mCasterContext.GetResults());
  }

  void Space::SetColliderEnabled(PhysicsObject& obj, bool enabled) {
    Collider* collider = obj.GetCollider();
    if(collider && !enabled)
      collider->Uninitialize(*this);
    else if(!collider && enabled)
      collider->Initialize(*this);
    obj.SetColliderEnabled(enabled);
  }

  void Space::SetRigidbodyEnabled(PhysicsObject& obj, bool enabled) {
    //Will probably soon have the same initialization as collider
    obj.SetRigidbodyEnabled(enabled);
  }

  Manifold* Space::GetManifold(PhysicsObject& a, PhysicsObject& b, ModelInstance& instA, ModelInstance& instB) {
    return mConstraintSystem.GetManifold(a, b, instA, instB);
  }

#define AddConstraint(func)\
    if(!FillOps(ops))\
      return SyxInvalidHandle;\
    return mConstraintSystem.func(ops)

  Handle Space::AddDistanceConstraint(DistanceOps& ops) {
    AddConstraint(AddDistanceConstraint);
  }

  Handle Space::AddSphericalConstraint(SphericalOps& ops) {
    ops.mObjA = mObjects.Get(ops.mA);
    ops.mObjB = mObjects.Get(ops.mB);
    if(!ops.mObjA || !ops.mObjB)
      return false;
    if(ops.mWorldAnchors) {
      SAlign Transformer worldToModelA = ops.mObjA->GetTransform().GetWorldToModel();
      SAlign Vec3 aligned = ops.mAnchorA;
      ops.mAnchorA = worldToModelA.TransformPoint(aligned);
      ops.mSwingFrame = ops.mObjA->GetTransform().mRot.Inversed()*ops.mSwingFrame;
      ops.mAnchorB = ops.mObjB->GetTransform().WorldToModel(ops.mAnchorB);
    }
    mConstraintSystem.AddSphericalConstraint(ops);
    return true;
  }

  Handle Space::AddWeldConstraint(WeldOps& ops) {
    AddConstraint(AddWeldConstraint);
  }

  Handle Space::AddRevoluteConstraint(RevoluteOps& ops) {
    ops.mObjA = mObjects.Get(ops.mA);
    ops.mObjB = mObjects.Get(ops.mB);
    if(!ops.mObjA || !ops.mObjB)
      return false;
    if(ops.mWorldAnchors) {
      SAlign Transformer worldToModelA = ops.mObjA->GetTransform().GetWorldToModel();
      SAlign Vec3 aligned = ops.mAnchorA;
      ops.mAnchorA = worldToModelA.TransformPoint(aligned);
      ops.mFreeAxis = ops.mObjA->GetTransform().mRot.Inversed()*ops.mFreeAxis;
      ops.mAnchorB = ops.mObjB->GetTransform().WorldToModel(ops.mAnchorB);
    }
    mConstraintSystem.AddRevoluteConstraint(ops);
    return true;
  }

  void Space::RemoveConstraint(Handle handle) {
    mConstraintSystem.RemoveConstraint(handle);
  }

  bool Space::FillOps(ConstraintOptions& ops) {
    ops.mObjA = mObjects.Get(ops.mA);
    ops.mObjB = mObjects.Get(ops.mB);
    if(!ops.mObjA || !ops.mObjB)
      return false;
    if(ops.mWorldAnchors) {
      ops.mAnchorA = ops.mObjA->GetTransform().WorldToModel(ops.mAnchorA);
      ops.mAnchorB = ops.mObjB->GetTransform().WorldToModel(ops.mAnchorB);
    }
    return true;
  }

#ifdef SENABLED
  void Space::SIntegrateAllPositions(float dt) {
    SFloats sdt = SLoadSplatFloats(dt);
    SFloats half = SLoadSplatFloats(0.5f);

    for(auto it = mObjects.Begin(); it != mObjects.End(); ++it) {
      PhysicsObject& obj = *it;
      Rigidbody* rigidbody = obj.GetRigidbody();
      //This ultimately shouldn't be needed because I should have a dynamic objects list that this would iterate over
      if(obj.GetAsleep() || !rigidbody || rigidbody->mInvMass < SYX_EPSILON)
        continue;

      Transform& t = obj.GetTransform();
      SFloats pos = ToSVec3(t.mPos);
      SFloats linVel = ToSVec3(rigidbody->mLinVel);
      pos = SAddAll(pos, SMulAll(linVel, sdt));
      SStoreAll(&t.mPos.x, pos);

      SFloats angVel(SLoadAll(&rigidbody->mAngVel.x));
      SFloats rot = ToSQuat(t.mRot);

      //The quaternion multiplication can be optimized because we know the w component is 0, removing a + and *
      SFloats spin = SQuat::MulVecQuat(half, SQuat::MulQuat(angVel, rot));
      rot = SQuat::Add(rot, SQuat::MulQuatVec(spin, sdt));
      rot = SQuat::Normalized(rot);

      SMat3 rotMat = SQuat::ToMatrix(rot);
      SStoreAll(&t.mRot.mV.x, rot);

      SFloats localInertia = ToSVec3(rigidbody->mLocalInertia);
      //Can probaby combine this whole operation so I don't need to store a transposed copy
      SMat3 transMat = rotMat.Transposed();
      //Scale matrix by inertia
      rotMat.mbx = SMulAll(rotMat.mbx, SShuffle(localInertia, 0, 0, 0, 0));
      rotMat.mby = SMulAll(rotMat.mby, SShuffle(localInertia, 1, 1, 1, 1));
      rotMat.mbz = SMulAll(rotMat.mbz, SShuffle(localInertia, 2, 2, 2, 2));
      rotMat *= transMat;
      //rotMat now contains Rot.Scaled(m_localInertia) * rot.Transposed
      rotMat.Store(rigidbody->mInvInertia);

      UpdateMovedObject(*it);
    }
  }

  void Space::SIntegrateAllVelocity(float dt) {
    SFloats gravity = SLoadSplatFloats(dt);
    gravity = SMulAll(gravity, SLoadFloats(0.0f, -10.0f, 0.0f));

    for(auto it = mObjects.Begin(); it != mObjects.End(); ++it) {
      PhysicsObject& obj = *it;
      if(obj.GetAsleep())
        continue;
      Rigidbody* rigidbody = obj.GetRigidbody();

      if(rigidbody && rigidbody->mInvMass > SYX_EPSILON) {
        SFloats sVel = ToSVec3(rigidbody->mLinVel);
        sVel = SAddAll(sVel, gravity);
        SStoreAll(&rigidbody->mLinVel.x, sVel);
      }
    }
  }

#else
  void Space::SIntegrateAllPositions(float) {}
  void Space::SIntegrateAllVelocity(float) {}
#endif
}