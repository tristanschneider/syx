#pragma once
#include "SyxWeldConstraint.h"
#include "SyxIslandGraph.h"
#include "SyxIslandSolver.h"
#include "SyxConstraintOptions.h"

namespace Syx {
  class Space;
  class Rigidbody;
  class Manifold;
  class PhysicsObject;

  class ConstraintSystem {
  public:
    static int sIterations;
    static float sEarlyOutThreshold;

    ConstraintSystem()
      : mIslandGraph(nullptr) {}

    void solve(float dt);
    void sSolve(float dt);

    void setIslandGraph(IslandGraph& graph) {
      mIslandGraph = &graph;
    }

    //Gets the existing manifold on the constraint between these two, or creates the constraint and returns the new manifold if there wasn't one
    Manifold* getManifold(PhysicsObject& objA, PhysicsObject& objB, ModelInstance& instA, ModelInstance& instB);
    //Update penetration values and discard invalid points
    //This won't be needed if objects know what constraints they have, because then it can be done when their position is integrated
    void updateManifolds(void);
    void clear(void);
    Constraint* getConstraint(Handle handle);

    Handle addDistanceConstraint(const DistanceOps& ops);
    Handle addSphericalConstraint(const SphericalOps& ops);
    Handle addWeldConstraint(const WeldOps& ops);
    Handle addRevoluteConstraint(const RevoluteOps& ops);

    void removeConstraint(Handle handle);
    void removeConstraint(Constraint& constraint);

  private:
    void _addConstraintMapping(Constraint& constraint);
    void _removeConstraintMapping(Constraint& constraint);
    void _moveConstraintMapping(Constraint& constraint);
    void _clearConstraintMappings();
    ContactConstraint* _createContact(PhysicsObject* objA, PhysicsObject* objB, Handle instA, Handle instB);
    void _removeContact(ContactConstraint& toRemove);
    void _addBlacklistPair(Handle a, Handle b);
    void _removeBlacklistPair(Handle a, Handle b);
    bool _isBlacklistPair(Handle a, Handle b);

    void _createSolvers();

    VecList<WeldConstraint> mWelds;
    VecList<ContactConstraint> mContacts;
    VecList<RevoluteConstraint> mRevolutes;
    VecList<DistanceConstraint> mDistances;
    VecList<SphericalConstraint> mSphericals;

    std::unordered_map<Handle, Constraint*> mHandleToConstraint;

    std::unordered_map<std::pair<Handle, Handle>, Manifold*, PairHash<Handle, Handle>> mPairToManifold;
    //Pairs that are not allowed to collide. int for every constraint preventing it.
    std::unordered_map<std::pair<Handle, Handle>, int, PairHash<Handle, Handle>> mCollisionBlacklist;
    IslandGraph* mIslandGraph;
    std::vector<IslandSolver, AlignmentAllocator<IslandSolver>> mSolvers;
    IslandContents mContents;
    HandleGenerator mConstraintHandleGen;
    size_t mSolverCount;
  };
}