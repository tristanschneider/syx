#pragma once
#include "SyxPhysicsObject.h"
#include "SyxConstraintSystem.h"
#include "SyxNarrowphase.h"
#include "SyxProfiler.h"
#include "SyxCaster.h"
#include "SyxIslandGraph.h"
#include "SyxEvents.h"

namespace Syx {
  class Broadphase;
  class BroadphaseContext;
  class Rigidbody;
  class PhysicsObject;
  class Collider;
  class Manifold;
  class NarrowphaseTest;
  class ModelInstance;

  class Space {
  public:
    friend Rigidbody;
    friend PhysicsObject;
    friend Collider;
    friend NarrowphaseTest;

    DeclareHandleMapNode(Space);

    Space(void);
    Space(Handle handle);
    Space(const Space& rhs);
    ~Space(void);

    Space& operator=(const Space& rhs);
    bool operator<(Handle rhs) { return mMyHandle < rhs; }
    bool operator==(Handle rhs) { return mMyHandle == rhs; }
    Handle getHandle(void) { return mMyHandle; }

    PhysicsObject* createObject(void);
    void destroyObject(Handle handle);
    PhysicsObject* getObject(Handle handle);

    const std::string& getProfileReport(const std::string& indent) { return mProfiler.getReport(indent); }
    const std::vector<ProfileResult>& getProfileHistory() { return mProfiler.getHistory(); }

    CastResult lineCastAll(const Vec3& start, const Vec3& end);

    void clear(void);
    void update(float dt);

    void wakeObject(PhysicsObject& obj);
    void updateMovedObject(PhysicsObject& obj);
    void setColliderEnabled(PhysicsObject& obj, bool enabled);
    void setRigidbodyEnabled(PhysicsObject& obj, bool enabled);

    Manifold* getManifold(PhysicsObject& a, PhysicsObject& b, ModelInstance& instA, ModelInstance& instB);

    Handle addDistanceConstraint(DistanceOps& ops);
    Handle addSphericalConstraint(SphericalOps& ops);
    Handle addWeldConstraint(WeldOps& ops);
    Handle addRevoluteConstraint(RevoluteOps& ops);

    void removeConstraint(Handle handle);

    const EventListener<UpdateEvent>& getUpdateEvents();

  private:
    bool _fillOps(ConstraintOptions& ops);

    void _integrateVelocity(float dt);
    void _integratePosition(float dt);
    void _collisionDetection(void);
    void _solveConstraints(float dt);

    void _integrateAllPositions(float dt);
    void _sIntegrateAllPositions(float dt);

    void _integrateAllVelocity(float dt);
    void _sIntegrateAllVelocity(float dt);

    void _fireUpdateEvent(PhysicsObject& obj);

    HandleMap<PhysicsObject> mObjects;
    Handle mMyHandle;
    Broadphase* mBroadphase;
    BroadphaseContext* mBroadphaseContext;
    Narrowphase mNarrowphase;
    ConstraintSystem mConstraintSystem;
    IslandGraph mIslandGraph;
    IslandContents mIslandStore;
    Caster mCaster;
    Profiler mProfiler;
    CasterContext mCasterContext;
    EventListener<UpdateEvent> mUpdateEvents;
  };
}