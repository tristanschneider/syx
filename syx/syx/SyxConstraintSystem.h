#pragma once
#include "SyxIntrusive.h"
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

    void Solve(float dt);
    void SSolve(float dt);

    void SetIslandGraph(IslandGraph& graph) {
      mIslandGraph = &graph;
    }

    //Gets the existing manifold on the constraint between these two, or creates the constraint and returns the new manifold if there wasn't one
    Manifold* GetManifold(PhysicsObject& objA, PhysicsObject& objB, ModelInstance& instA, ModelInstance& instB);
    //Update penetration values and discard invalid points
    //This won't be needed if objects know what constraints they have, because then it can be done when their position is integrated
    void UpdateManifolds(void);
    void Clear(void);
    Constraint* GetConstraint(Handle handle);

    Handle AddDistanceConstraint(const DistanceOps& ops);
    Handle AddSphericalConstraint(const SphericalOps& ops);
    Handle AddWeldConstraint(const WeldOps& ops);
    Handle AddRevoluteConstraint(const RevoluteOps& ops);

    void RemoveConstraint(Handle handle);

  private:
    void AddConstraintMapping(Constraint& constraint);
    void RemoveConstraintMapping(Constraint& constraint);
    void MoveConstraintMapping(Constraint& constraint);
    void ClearConstraintMappings();
    ContactConstraint* CreateContact(PhysicsObject* objA, PhysicsObject* objB, Handle instA, Handle instB);
    void RemoveContact(ContactConstraint& toRemove);
    void AddBlacklistPair(Handle a, Handle b);
    void RemoveBlacklistPair(Handle a, Handle b);
    bool IsBlacklistPair(Handle a, Handle b);

    void CreateSolvers();

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