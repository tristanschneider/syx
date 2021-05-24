#pragma once
#include "SyxAlignmentAllocator.h"
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
  class Collider;
  struct IPhysicsObject;
  class Manifold;
  class ModelInstance;
  class NarrowphaseTest;
  class PhysicsObject;
  class Rigidbody;

  struct ISpace {
    virtual ~ISpace() = default;

    virtual const EventListener<UpdateEvent>& getUpdateEvents() const = 0;
    virtual void clear() = 0;
    //TODO: this should be removed once all necessary operations are possible through ISpace
    virtual Handle _getHandle() const = 0;
    virtual std::shared_ptr<IPhysicsObject> addPhysicsObject(bool hasRigidbody, bool hasCollider, const IMaterialHandle& material, std::shared_ptr<const Model> model) = 0;
    virtual void garbageCollect() = 0;
  };

  class Space : public ISpace {
  public:
    friend Rigidbody;
    friend PhysicsObject;
    friend Collider;
    friend NarrowphaseTest;

    DeclareHandleMapNode(Space);

    Space(Handle handle = 0);
    Space(const Space& rhs);
    ~Space(void);

    Space& operator=(const Space& rhs);
    bool operator<(Handle rhs) { return mMyHandle < rhs; }
    bool operator==(Handle rhs) { return mMyHandle == rhs; }
    Handle getHandle() const { return mMyHandle; }
    Handle _getHandle() const override { return getHandle(); }

    PhysicsObject* getObject(Handle handle);

    const std::string& getProfileReport(const std::string& indent) { return mProfiler.getReport(indent); }
    const std::vector<ProfileResult>& getProfileHistory() { return mProfiler.getHistory(); }

    CastResult lineCastAll(const Vec3& start, const Vec3& end);

    std::shared_ptr<IPhysicsObject> addPhysicsObject(bool hasRigidbody, bool hasCollider, const IMaterialHandle& material, std::shared_ptr<const Model> model) override;

    void clear(void) override;
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

    const EventListener<UpdateEvent>& getUpdateEvents() const override;

    void garbageCollect() override;

  private:
    void _destroyObject(PhysicsObject& obj);
    bool _fillOps(ConstraintOptions& ops);
    void _garbageCollect();

    void _integrateVelocity(float dt);
    void _integratePosition(float dt);
    void _collisionDetection(void);
    void _solveConstraints(float dt);

    void _integrateAllPositions(float dt);
    void _sIntegrateAllPositions(float dt);

    void _integrateAllVelocity(float dt);
    void _sIntegrateAllVelocity(float dt);

    void _fireUpdateEvent(PhysicsObject& obj);

    std::vector<PhysicsObject, AlignmentAllocator<PhysicsObject>> mObjects;
    Handle mMyHandle;
    std::unique_ptr<Broadphase> mBroadphase;
    std::unique_ptr<BroadphaseContext> mBroadphaseContext;
    std::unique_ptr<BroadphasePairContext> mBroadphasePairContext;
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