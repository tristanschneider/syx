#pragma once
#include "SyxContactConstraint.h"
#include "SyxDistanceConstraint.h"
#include "SyxSphericalConstraint.h"
#include "SyxWeldConstraint.h"
#include "SyxRevoluteConstraint.h"
#include "SyxIslandGraph.h"

namespace Syx {
  SAlign class IslandSolver {
  public:
    void set(const IslandContents& island);
    void solve(float dt);
    void sSolve(float dt);
    void preSolve(float dt);
    void postSolve();
    void storeObjects();
    const std::vector<Constraint*>& getToRemove();
    IndexableKey getIslandKey();
    SleepState getNewIslandState();

  private:
    size_t _getObjectIndex(PhysicsObject& obj);
    void _pushLocalConstraint(Constraint& constraint, size_t indexA, size_t indexB);
    void _clearLocalConstraints();

    std::vector<LocalObject, AlignmentAllocator<LocalObject>> mObjects;
    std::vector<LocalWeldConstraint, AlignmentAllocator<LocalWeldConstraint>> mWelds;
    std::vector<LocalRevoluteConstraint, AlignmentAllocator<LocalRevoluteConstraint>> mRevolutes;
    std::vector<LocalContactConstraint, AlignmentAllocator<LocalContactConstraint>> mContacts;
    std::vector<LocalSphericalConstraint, AlignmentAllocator<LocalSphericalConstraint>> mSphericals;
    std::vector<LocalDistanceConstraint, AlignmentAllocator<LocalDistanceConstraint>> mDistances;
    std::vector<Constraint*> mToRemove;
    std::unordered_map<Handle, size_t> mObjHandleToIndex;
    IndexableKey mIslandKey;
    SleepState mNewIslandState;
    SleepState mCurIslandState;
    SPadClass(SVectorSize*7 + SMapSize + sizeof(IndexableKey) + sizeof(SleepState)*2);
  };

}