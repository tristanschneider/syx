#pragma once
#include "SyxIntrusive.h"
#include "SyxContactConstraint.h"
#include "SyxDistanceConstraint.h"
#include "SyxSphericalConstraint.h"
#include "SyxWeldConstraint.h"
#include "SyxRevoluteConstraint.h"
#include "SyxIslandGraph.h"

namespace Syx {
  SAlign class IslandSolver {
  public:
    void Set(const IslandContents& island);
    void Solve(float dt);
    void SSolve(float dt);
    void PreSolve(float dt);
    void PostSolve();
    void StoreObjects();
    const std::vector<Constraint*>& GetToRemove();
    IndexableKey GetIslandKey();
    SleepState GetNewIslandState();

  private:
    size_t GetObjectIndex(PhysicsObject& obj);
    void PushLocalConstraint(Constraint& constraint, size_t indexA, size_t indexB);
    void ClearLocalConstraints();

    std::vector<LocalObject, AlignmentAllocator<LocalObject>> mObjects;
    std::vector<LocalWeldConstraint, AlignmentAllocator<LocalContactConstraint>> mWelds;
    std::vector<LocalRevoluteConstraint, AlignmentAllocator<RevoluteConstraint>> mRevolutes;
    std::vector<LocalContactConstraint, AlignmentAllocator<LocalContactConstraint>> mContacts;
    std::vector<LocalSphericalConstraint, AlignmentAllocator<SphericalConstraint>> mSphericals;
    std::vector<LocalDistanceConstraint, AlignmentAllocator<LocalDistanceConstraint>> mDistances;
    std::vector<Constraint*> mToRemove;
    std::unordered_map<Handle, size_t> mObjHandleToIndex;
    IndexableKey mIslandKey;
    SleepState mNewIslandState;
    SleepState mCurIslandState;
    SPadClass(SVectorSize*7 + SMapSize + sizeof(IndexableKey) + sizeof(SleepState)*2);
  };

}