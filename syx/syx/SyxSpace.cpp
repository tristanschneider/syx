#include "Precompile.h"
#include "SyxSpace.h"

#include "SyxAABBTree.h"
#include "SyxIPhysicsObject.h"

namespace Syx {
  //Temporary until handle access is removed in favor of using IPhysicsObject directly
  HandleGenerator HANDLE_GEN;

  Space::Space(Handle handle)
    : mMyHandle(handle)
    , mBroadphase(Create::aabbTree())
    , mBroadphaseContext(mBroadphase->createHitContext())
    , mBroadphasePairContext(mBroadphase->createPairContext()) {
    mConstraintSystem.setIslandGraph(mIslandGraph);
  }

  Space::Space(const Space& rhs) {
    *this = rhs;
    mObjects.reserve(100);
    mBroadphase = Create::aabbTree();
    mBroadphaseContext = mBroadphase->createHitContext();
    mBroadphasePairContext = mBroadphase->createPairContext();
  }

  Space::~Space() {
  }

  Space& Space::operator=(const Space& rhs) {
    *mBroadphase = *rhs.mBroadphase;
    mConstraintSystem = rhs.mConstraintSystem;
    mMyHandle = rhs.mMyHandle;
    mNarrowphase = rhs.mNarrowphase;
    //TODO: either implement or delete assignment
    //mObjects = rhs.mObjects;
    mProfiler = rhs.mProfiler;
    mConstraintSystem.setIslandGraph(mIslandGraph);
    mBroadphase = Create::aabbTree();
    mBroadphaseContext = mBroadphase->createHitContext();
    mBroadphasePairContext = mBroadphase->createPairContext();
    return *this;
  }

  void Space::_destroyObject(PhysicsObject& obj) {
    mIslandGraph.remove(obj, [this](Constraint& c) {
      mConstraintSystem.removeConstraint(c);
    });
    //TODO: sketchy, should be part of destructor or otherwise properly scoped
    if(Collider* c = obj.getCollider()) {
      c->uninitialize(*this);
    }
  }

  PhysicsObject* Space::getObject(Handle handle) {
    auto it = std::find_if(mObjects.begin(), mObjects.end(), [handle](const PhysicsObject& obj) {
      return obj.getHandle() == handle;
    });
    return it != mObjects.end() ? &*it : nullptr;
  }

  void Space::clear(void) {
    mObjects.clear();
    mBroadphase->clear();
    mConstraintSystem.clear();
    mIslandGraph.clear();
  }

  void Space::_garbageCollect() {
    AutoProfileBlock block(mProfiler, "GC");
    for(size_t i = 0; i < mObjects.size(); ++i) {
      if(PhysicsObject& curObj = mObjects[i]; curObj.isMarkedForDeletion()) {
        _destroyObject(curObj);
        //Swap remove
        if(mObjects.size() > 1 && i + 1 != mObjects.size()) {
           curObj = std::move(mObjects.back());
        }
        mObjects.pop_back();
      }
      else {
        ++i;
      }
    }
  }

  void Space::update(float dt) {
    AutoProfileBlock block(mProfiler, "Update Space");

    _garbageCollect();
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
      if(Interface::getOptions().mDebugFlags & (SyxOptions::Debug::DrawModels | SyxOptions::Debug::DrawPersonalBBs | SyxOptions::Debug::DrawCenterOfMass))
        for(auto it = mObjects.begin(); it != mObjects.end(); ++it) {
          PhysicsObject& obj = *it;
          d.setColor(color.x, color.y, color.z);
          obj.drawModel();
          if((Interface::getOptions().mDebugFlags & SyxOptions::Debug::DrawSleeping) && obj.getAsleep()) {
            d.setColor(1.0f, 1.0f, 1.0f);
            d.drawPoint(obj.getTransform().mPos, 0.1f);
          }
          if((Interface::getOptions().mDebugFlags & SyxOptions::Debug::DrawCenterOfMass)) {
            d.setColor(1.0f, 1.0f, 0.0f);
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
    mBroadphasePairContext->queryPairs();
    mProfiler.popBlock("Broadphase");

    {
      AutoProfileBlock narrow(mProfiler, "Narrowphase");
      mNarrowphase.processPairQuery(mBroadphasePairContext->get(), *this);
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
    mBroadphaseContext->queryRaycast(start, end);
    mCasterContext.clearResults();

    for(const ResultNode& obj : mBroadphaseContext->get()) {
      PhysicsObject* pObj = PhysicsObject::_fromUserdata(obj.mUserdata);
      mCaster.lineCast(*pObj, start, end, mCasterContext);
    }

    mCasterContext.sortResults();
    return CastResult(&mCasterContext.getResults());
  }

  void Space::setColliderEnabled(PhysicsObject& obj, bool enabled) {
    Collider* collider = obj.getCollider();
    if(collider && !enabled) {
      collider->uninitialize(*this);
      obj.setColliderEnabled(false);
    }
    else if(!collider && enabled) {
      obj.setColliderEnabled(true);
      obj.getCollider()->initialize(*this);
    }
  }

  void Space::setRigidbodyEnabled(PhysicsObject& obj, bool enabled) {
    //Will probably soon have the same initialization as collider
    obj.setRigidbodyEnabled(enabled);
    if(Rigidbody* body = obj.getRigidbody()) {
      body->calculateMass();
    }
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
    ops.mObjA = getObject(ops.mA);
    ops.mObjB = getObject(ops.mB);
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
    ops.mObjA = getObject(ops.mA);
    ops.mObjB = getObject(ops.mB);
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

  const EventListener<UpdateEvent>& Space::getUpdateEvents() const {
    return mUpdateEvents;
  }

  void Space::garbageCollect() {
    _garbageCollect();
  }

  bool Space::_fillOps(ConstraintOptions& ops) {
    ops.mObjA = getObject(ops.mA);
    ops.mObjB = getObject(ops.mB);
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

  std::shared_ptr<IPhysicsObject> Space::addPhysicsObject(bool hasRigidbody, bool hasCollider, const IMaterialHandle& material, std::shared_ptr<const Model> model) {
    DeferredDeleteResourceHandle<PhysicsObject> handle;
    mObjects.emplace_back(HANDLE_GEN.next(), handle);
    PhysicsObject& newObj = mObjects.back();

    //Enable so we can set defaults
    newObj.setColliderEnabled(true);
    newObj.getCollider()->setMaterial(material);
    newObj.getCollider()->setModel(std::move(model));
    newObj.updateModelInst();
    newObj.setColliderEnabled(hasCollider);
    if(hasCollider)
      newObj.getCollider()->initialize(*this);

    setRigidbodyEnabled(newObj, hasRigidbody);

    newObj.updateModelInst();

    if(hasRigidbody) {
      newObj.getRigidbody()->calculateMass();
    }

    return createPhysicsObjectRef(handle, *this);
  }

#else
  void Space::_sIntegrateAllPositions(float) {}
  void Space::_sIntegrateAllVelocity(float) {}
#endif
}