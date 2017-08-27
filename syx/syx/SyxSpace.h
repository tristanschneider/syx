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
    Handle GetHandle(void) { return mMyHandle; }

    PhysicsObject* CreateObject(void);
    void DestroyObject(Handle handle);
    PhysicsObject* GetObject(Handle handle);

    const std::string& GetProfileReport(const std::string& indent) { return mProfiler.GetReport(indent); }
    const std::vector<ProfileResult>& GetProfileHistory() { return mProfiler.GetHistory(); }

    CastResult LineCastAll(const Vec3& start, const Vec3& end);

    void Clear(void);
    void Update(float dt);

    void WakeObject(PhysicsObject& obj);
    void UpdateMovedObject(PhysicsObject& obj);
    void SetColliderEnabled(PhysicsObject& obj, bool enabled);
    void SetRigidbodyEnabled(PhysicsObject& obj, bool enabled);

    Manifold* GetManifold(PhysicsObject& a, PhysicsObject& b, ModelInstance& instA, ModelInstance& instB);

    Handle AddDistanceConstraint(DistanceOps& ops);
    Handle AddSphericalConstraint(SphericalOps& ops);
    Handle AddWeldConstraint(WeldOps& ops);
    Handle AddRevoluteConstraint(RevoluteOps& ops);

    void RemoveConstraint(Handle handle);

    const EventListener<UpdateEvent>& getUpdateEvents();

  private:
    bool FillOps(ConstraintOptions& ops);

    void IntegrateVelocity(float dt);
    void IntegratePosition(float dt);
    void CollisionDetection(void);
    void SolveConstraints(float dt);

    void IntegrateAllPositions(float dt);
    void SIntegrateAllPositions(float dt);

    void IntegrateAllVelocity(float dt);
    void SIntegrateAllVelocity(float dt);

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