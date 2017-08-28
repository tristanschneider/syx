#include "Precompile.h"
#include "SyxSpace.h"
#include "SyxAABBTree.h"

namespace Syx {
  typedef AABBTree DefaultBroadphase;
  typedef AABBTreeContext DefaultBroadphaseContext;

  Space::Space(void)
    : mBroadphase(new DefaultBroadphase())
    , mBroadphaseContext(new DefaultBroadphaseContext()) {
    mConstraintSystem.setIslandGraph(mIslandGraph);
  }

  Space::Space(Handle handle)
    : mMyHandle(handle)
    , mBroadphase(new DefaultBroadphase())
    , mBroadphaseContext(new DefaultBroadphaseContext()) {
    mConstraintSystem.setIslandGraph(mIslandGraph);
  }

  Space::Space(const Space& rhs) {
    *this = rhs;
    mObjects.reserve(100);
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
    mConstraintSystem.setIslandGraph(mIslandGraph);
    return *this;
  }

  PhysicsObject* Space::createObject(void) {
    return mObjects.add();
  }

  void Space::destroyObject(Handle handle) {
    PhysicsObject* obj = mObjects.get(handle);
    while(!obj->getConstraints().empty())
      removeConstraint(*obj->getConstraints().begin());
    mIslandGraph.remove(*mObjects.get(handle));
    mObjects.remove(handle);
  }

  PhysicsObject* Space::getObject(Handle handle) {
    return mObjects.get(handle);
  }

  void Space::clear(void) {
    mObjects.clear();
    mBroadphase->clear();
    mConstraintSystem.clear();
    mIslandGraph.clear();
  }

  void Space::update(float dt) {
    AutoProfileBlock block(mProfiler, "Update Space");

    _integrateVelocity(dt);
    if((Interface::getOptions().mDebugFlags & SyxOptions::DisableCollision) == 0)
      _collisionDetection();
    _solveConstraints(dt);
    _integratePosition(dt);
  }

  void Space::_integrateVelocity(float dt) {
    AutoProfileBlock block(mProfiler, "Integrate Velocity");
    if(Interface::getOptions().mSimdFlags & SyxOptions::SIMD::VelocityIntegration)
      _sIntegrateAllVelocity(dt);
    else
      _integrateAllVelocity(dt);
  }

  void Space::_integrateAllVelocity(float dt) {
    for(auto it = mObjects.begin(); it != mObjects.end(); ++it) {
      PhysicsObject& obj = *it;
      if(obj.getAsleep())
        continue;
      Rigidbody* rigidbody = obj.getRigidbody();
      if(rigidbody)
        rigidbody->integrateVelocity(dt);
    }
  }

  void Space::updateMovedObject(PhysicsObject& obj) {
    obj.updateModelInst();
    Collider* collider = obj.getCollider();
    if(collider)
      collider->mBroadHandle = mBroadphase->update(BoundingVolume(collider->getAABB()), collider->mBroadHandle);
  }

  void Space::wakeObject(PhysicsObject& obj) {
    mIslandGraph.wakeIsland(obj);
  }

  void Space::_integratePosition(float dt) {
    {
      AutoProfileBlock block(mProfiler, "Integrate Position");
      mUpdateEvents.mEvents.clear();
      if(Interface::getOptions().mSimdFlags & SyxOptions::SIMD::PositionIntegration)
        _sIntegrateAllPositions(dt);
      else
        _integrateAllPositions(dt);
    }

    {
      AutoProfileBlock block(mProfiler, "Update Manifolds");
      mConstraintSystem.updateManifolds();
    }

    {
      DebugDrawer& d = DebugDrawer::get();
      AutoProfileBlock draw(mProfiler, "Debug Draw");
      Vec3 color = Vec3::Zero;
      color[mMyHandle % 3] = 1.0f;
      if(Interface::getOptions().mDebugFlags & (SyxOptions::Debug::DrawModels | SyxOptions::Debug::DrawPersonalBBs))
        for(auto it = mObjects.begin(); it != mObjects.end(); ++it) {
          PhysicsObject& obj = *it;
          d.setColor(color.x, color.y, color.z);
          obj.drawModel();
          if((Interface::getOptions().mDebugFlags & SyxOptions::Debug::DrawSleeping) && obj.getAsleep()) {
            d.setColor(1.0f, 1.0f, 1.0f);
            d.drawPoint(obj.getTransform().mPos, 0.1f);
          }
        }

      if(Interface::getOptions().mDebugFlags & SyxOptions::Debug::DrawBroadphase)
        mBroadphase->draw();

      if(Interface::getOptions().mDebugFlags & SyxOptions::Debug::DrawIslands) {
        Vec3 colors[] = {Vec3::Zero
          , Vec3::UnitX
          , Vec3::UnitY
          , Vec3::UnitZ
          , Vec3::UnitX + Vec3::UnitY
          , Vec3::UnitX + Vec3::UnitZ
          , Vec3::UnitY + Vec3::UnitZ
          , Vec3::Identity};
        size_t colorCount = 8;

        for(size_t i = 0; i < mIslandGraph.islandCount(); ++i) {
          Vec3 c = colors[i % colorCount];
          d.setColor(c.x, c.y, c.z);
          mIslandGraph.getIsland(i, mIslandStore, true);
          for(Constraint* constraint : mIslandStore.mConstraints) {
            //Don't draw static object lines as it clutters visuals quickly for ground planes
            if(!constraint->getObjA()->isStatic() && !constraint->getObjB()->isStatic())
              d.drawLine(constraint->getObjA()->getTransform().mPos, constraint->getObjB()->getTransform().mPos);
          }
        }
      }
    }
  }

  void Space::_integrateAllPositions(float dt) {
    for(auto it = mObjects.begin(); it != mObjects.end(); ++it) {
      PhysicsObject& obj = *it;
      if(obj.getAsleep())
        continue;
      Rigidbody* rigidbody = obj.getRigidbody();
      if(obj.shouldIntegrate()) {
        obj.getRigidbody()->integratePosition(dt);
        _fireUpdateEvent(obj);
        updateMovedObject(*it);
      }
    }
  }

  void Space::_collisionDetection(void) {
    AutoProfileBlock detection(mProfiler, "Collision Detection");

    mProfiler.pushBlock("Broadphase");
    mBroadphase->queryPairs(*mBroadphaseContext);
    mProfiler.popBlock("Broadphase");

    {
      AutoProfileBlock narrow(mProfiler, "Narrowphase");
      mNarrowphase.processPairQuery(mBroadphaseContext->mQueryPairResults, *this);
    }
  }

  void Space::_solveConstraints(float dt) {
    AutoProfileBlock block(mProfiler, "Constraint Solving");
    if(gOptions.mSimdFlags & SyxOptions::SIMD::ConstraintSolve)
      mConstraintSystem.sSolve(dt);
    else
      mConstraintSystem.solve(dt);
  }

  CastResult Space::lineCastAll(const Vec3& start, const Vec3& end) {
    mBroadphase->queryRaycast(start, end, *mBroadphaseContext);
    mCasterContext.clearResults();

    for(const ResultNode& obj : mBroadphaseContext->mQueryResults) {
      PhysicsObject* pObj = reinterpret_cast<PhysicsObject*>(obj.mUserdata);
      mCaster.lineCast(*pObj, start, end, mCasterContext);
    }

    mCasterContext.sortResults();
    return CastResult(&mCasterContext.getResults());
  }

  void Space::setColliderEnabled(PhysicsObject& obj, bool enabled) {
    Collider* collider = obj.getCollider();
    if(collider && !enabled)
      collider->uninitialize(*this);
    else if(!collider && enabled)
      collider->initialize(*this);
    obj.setColliderEnabled(enabled);
  }

  void Space::setRigidbodyEnabled(PhysicsObject& obj, bool enabled) {
    //Will probably soon have the same initialization as collider
    obj.setRigidbodyEnabled(enabled);
  }

  Manifold* Space::getManifold(PhysicsObject& a, PhysicsObject& b, ModelInstance& instA, ModelInstance& instB) {
    return mConstraintSystem.getManifold(a, b, instA, instB);
  }

#define AddConstraint(func)\
    if(!_fillOps(ops))\
      return SyxInvalidHandle;\
    return mConstraintSystem.func(ops)

  Handle Space::addDistanceConstraint(DistanceOps& ops) {
    AddConstraint(addDistanceConstraint);
  }

  Handle Space::addSphericalConstraint(SphericalOps& ops) {
    ops.mObjA = mObjects.get(ops.mA);
    ops.mObjB = mObjects.get(ops.mB);
    if(!ops.mObjA || !ops.mObjB)
      return false;
    if(ops.mWorldAnchors) {
      SAlign Transformer worldToModelA = ops.mObjA->getTransform().getWorldToModel();
      SAlign Vec3 aligned = ops.mAnchorA;
      ops.mAnchorA = worldToModelA.transformPoint(aligned);
      ops.mSwingFrame = ops.mObjA->getTransform().mRot.inversed()*ops.mSwingFrame;
      ops.mAnchorB = ops.mObjB->getTransform().worldToModel(ops.mAnchorB);
    }
    mConstraintSystem.addSphericalConstraint(ops);
    return true;
  }

  Handle Space::addWeldConstraint(WeldOps& ops) {
    AddConstraint(addWeldConstraint);
  }

  Handle Space::addRevoluteConstraint(RevoluteOps& ops) {
    ops.mObjA = mObjects.get(ops.mA);
    ops.mObjB = mObjects.get(ops.mB);
    if(!ops.mObjA || !ops.mObjB)
      return false;
    if(ops.mWorldAnchors) {
      SAlign Transformer worldToModelA = ops.mObjA->getTransform().getWorldToModel();
      SAlign Vec3 aligned = ops.mAnchorA;
      ops.mAnchorA = worldToModelA.transformPoint(aligned);
      ops.mFreeAxis = ops.mObjA->getTransform().mRot.inversed()*ops.mFreeAxis;
      ops.mAnchorB = ops.mObjB->getTransform().worldToModel(ops.mAnchorB);
    }
    mConstraintSystem.addRevoluteConstraint(ops);
    return true;
  }

  void Space::removeConstraint(Handle handle) {
    mConstraintSystem.removeConstraint(handle);
  }

  const EventListener<UpdateEvent>& Space::getUpdateEvents() {
    return mUpdateEvents;
  }

  bool Space::_fillOps(ConstraintOptions& ops) {
    ops.mObjA = mObjects.get(ops.mA);
    ops.mObjB = mObjects.get(ops.mB);
    if(!ops.mObjA || !ops.mObjB)
      return false;
    if(ops.mWorldAnchors) {
      ops.mAnchorA = ops.mObjA->getTransform().worldToModel(ops.mAnchorA);
      ops.mAnchorB = ops.mObjB->getTransform().worldToModel(ops.mAnchorB);
    }
    return true;
  }

#ifdef SENABLED
  void Space::_sIntegrateAllPositions(float dt) {
    SFloats sdt = sLoadSplatFloats(dt);
    SFloats half = sLoadSplatFloats(0.5f);

    for(auto it = mObjects.begin(); it != mObjects.end(); ++it) {
      PhysicsObject& obj = *it;
      Rigidbody* rigidbody = obj.getRigidbody();
      //This ultimately shouldn't be needed because I should have a dynamic objects list that this would iterate over
      if(!obj.shouldIntegrate())
        continue;

      Transform& t = obj.getTransform();
      SFloats pos = toSVec3(t.mPos);
      SFloats linVel = toSVec3(rigidbody->mLinVel);
      pos = SAddAll(pos, SMulAll(linVel, sdt));
      SStoreAll(&t.mPos.x, pos);

      SFloats angVel(SLoadAll(&rigidbody->mAngVel.x));
      SFloats rot = toSQuat(t.mRot);

      //The quaternion multiplication can be optimized because we know the w component is 0, removing a + and *
      SFloats spin = SQuat::mulVecQuat(half, SQuat::mulQuat(angVel, rot));
      rot = SQuat::add(rot, SQuat::mulQuatVec(spin, sdt));
      rot = SQuat::normalized(rot);

      SMat3 rotMat = SQuat::toMatrix(rot);
      SStoreAll(&t.mRot.mV.x, rot);

      SFloats localInertia = toSVec3(rigidbody->mLocalInertia);
      //Can probaby combine this whole operation so I don't need to store a transposed copy
      SMat3 transMat = rotMat.transposed();
      //Scale matrix by inertia
      rotMat.mbx = SMulAll(rotMat.mbx, SShuffle(localInertia, 0, 0, 0, 0));
      rotMat.mby = SMulAll(rotMat.mby, SShuffle(localInertia, 1, 1, 1, 1));
      rotMat.mbz = SMulAll(rotMat.mbz, SShuffle(localInertia, 2, 2, 2, 2));
      rotMat *= transMat;
      //rotMat now contains Rot.Scaled(m_localInertia) * rot.Transposed
      rotMat.store(rigidbody->mInvInertia);

      _fireUpdateEvent(obj);
      updateMovedObject(*it);
    }
  }

  void Space::_sIntegrateAllVelocity(float dt) {
    SFloats gravity = sLoadSplatFloats(dt);
    gravity = SMulAll(gravity, sLoadFloats(0.0f, -10.0f, 0.0f));

    for(auto it = mObjects.begin(); it != mObjects.end(); ++it) {
      PhysicsObject& obj = *it;
      if(obj.getAsleep())
        continue;
      Rigidbody* rigidbody = obj.getRigidbody();

      if(rigidbody && rigidbody->mInvMass > SYX_EPSILON) {
        SFloats sVel = toSVec3(rigidbody->mLinVel);
        sVel = SAddAll(sVel, gravity);
        SStoreAll(&rigidbody->mLinVel.x, sVel);
      }
    }
  }

  void Space::_fireUpdateEvent(PhysicsObject& obj) {
    UpdateEvent e;
    //Can't be null because if it was it wouldn't move, so we wouldn't fire this event
    Rigidbody* rb = obj.getRigidbody();
    e.mLinVel = rb->mLinVel;
    e.mAngVel = rb->mAngVel;
    e.mPos = obj.getTransform().mPos;
    e.mRot = obj.getTransform().mRot;
    e.mHandle = obj.getHandle();
    mUpdateEvents.mEvents.push_back(e);
  }

#else
  void Space::_sIntegrateAllPositions(float) {}
  void Space::_sIntegrateAllVelocity(float) {}
#endif
}