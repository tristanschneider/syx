#include "Precompile.h"
#include "SyxConstraintSystem.h"

namespace Syx {
  int ConstraintSystem::sIterations = 10;
  float ConstraintSystem::sEarlyOutThreshold = 0.00001f;

  void ConstraintSystem::Solve(float dt) {
    CreateSolvers();
    for(size_t i = 0; i < mSolverCount; ++i) {
      IslandSolver& solver = mSolvers[i];
      solver.Solve(dt);
      for(Constraint* toRemove : solver.GetToRemove())
        RemoveContact(*static_cast<ContactConstraint*>(toRemove));
      mIslandGraph->UpdateIslandState(solver.GetIslandKey(), solver.GetNewIslandState(), dt);
    }
  }

  void ConstraintSystem::SSolve(float dt) {
    CreateSolvers();
    for(size_t i = 0; i < mSolverCount; ++i) {
      IslandSolver& solver = mSolvers[i];
      solver.SSolve(dt);
      for(Constraint* toRemove : solver.GetToRemove())
        RemoveContact(*static_cast<ContactConstraint*>(toRemove));
      mIslandGraph->UpdateIslandState(solver.GetIslandKey(), solver.GetNewIslandState(), dt);
    }
  }

  Manifold* ConstraintSystem::GetManifold(PhysicsObject& objA, PhysicsObject& objB, ModelInstance& instA, ModelInstance& instB) {
    size_t oldSize = mPairToManifold.size();
    std::pair<Handle, Handle> pair = {instA.GetHandle(), instB.GetHandle()};
    Manifold*& foundManifold = mPairToManifold[pair];

    //Size didn't change, meaning nothing was inserted, so we found a manifold
    if(oldSize == mPairToManifold.size())
      return foundManifold;

    //There's going to be many more whitelisted pairs than blacklisted, so optimize for former
    if(IsBlacklistPair(objA.GetHandle(), objB.GetHandle())) {
      mPairToManifold.erase(pair);
      return nullptr;
    }

    //Didn't find a manifold. Make one and return it
    ContactConstraint* newConstraint = CreateContact(&objA, &objB, instA.GetHandle(), instB.GetHandle());
    foundManifold = &newConstraint->mManifold;
    mIslandGraph->Add(*newConstraint);
    return foundManifold;
  }

  void ConstraintSystem::UpdateManifolds(void) {
    for(auto it = mContacts.Begin(); it != mContacts.End(); ++it)
      (*it).mManifold.Update();
  }

  void ConstraintSystem::Clear(void) {
    mContacts.Clear();
    mDistances.Clear();
    mRevolutes.Clear();
    mSphericals.Clear();
    mWelds.Clear();
    mPairToManifold.clear();
    ClearConstraintMappings();
    mCollisionBlacklist.clear();
  }

  void ConstraintSystem::CreateSolvers() {
    mSolverCount = mIslandGraph->IslandCount();
    //Don't want to resize down because that would destruct solvers whose memory could be re-used later
    if(mSolvers.size() < mSolverCount) {
      mSolvers.resize(mSolverCount);
    }

    for(size_t i = 0; i < mSolverCount; ++i) {
      mIslandGraph->GetIsland(i, mContents);
      mSolvers[i].Set(mContents);
    }
  }

  ContactConstraint* ConstraintSystem::CreateContact(PhysicsObject* objA, PhysicsObject* objB, Handle instA, Handle instB) {
    ContactConstraint* result = mContacts.Push(ContactConstraint(objA, objB, mConstraintHandleGen.Next(), instA, instB));
    SyxAssertError(mHandleToConstraint.find(result->GetHandle()) == mHandleToConstraint.end(), "Created new constraint with duplicate handle");
    return result;
  }

  static void SetAnchors(const ConstraintOptions& ops, Constraint& constraint) {
    constraint.SetLocalAnchor(ops.mAnchorA, ConstraintObj::A);
    constraint.SetLocalAnchor(ops.mAnchorB, ConstraintObj::B);
  }

  Handle ConstraintSystem::AddDistanceConstraint(const DistanceOps& ops) {
    DistanceConstraint* result = mDistances.Push(DistanceConstraint(ops.mObjA, ops.mObjB, mConstraintHandleGen.Next()));
    SetAnchors(ops, *result);
    result->SetDistance(ops.mDistance);
    result->SetBlacklistCollision(!ops.mCollisionEnabled);
    AddConstraintMapping(*result);
    return result->GetHandle();
  }

  Handle ConstraintSystem::AddSphericalConstraint(const SphericalOps& ops) {
    SphericalConstraint* result = mSphericals.Push(SphericalConstraint(ops.mObjA, ops.mObjB, mConstraintHandleGen.Next()));
    SetAnchors(ops, *result);
    result->SetSwingFrame(ops.mSwingFrame);
    result->SetSwingLimits(ops.mMaxSwingRadsX, ops.mMaxSwingRadsY);
    result->SetTwistLimits(ops.mMinTwistRads, ops.mMaxTwistRads);
    result->SetMaxAngularImpulse(ops.mMaxAngularImpulse);
    result->SetBlacklistCollision(!ops.mCollisionEnabled);
    AddConstraintMapping(*result);
    return result->GetHandle();
  }

  Handle ConstraintSystem::AddWeldConstraint(const WeldOps& ops) {
    WeldConstraint* result = mWelds.Push(WeldConstraint(ops.mObjA, ops.mObjB, mConstraintHandleGen.Next()));
    result->LockRelativeTransform();
    result->SetBlacklistCollision(!ops.mCollisionEnabled);
    AddConstraintMapping(*result);
    return result->GetHandle();
  }

  Handle ConstraintSystem::AddRevoluteConstraint(const RevoluteOps& ops) {
    RevoluteConstraint* result = mRevolutes.Push(RevoluteConstraint(ops.mObjA, ops.mObjB, mConstraintHandleGen.Next()));
    SetAnchors(ops, *result);
    result->SetFreeLimits(ops.mMinRads, ops.mMaxRads);
    result->SetLocalFreeAxis(ops.mFreeAxis);
    result->SetMaxFreeImpulse(ops.mMaxFreeImpulse);
    result->SetBlacklistCollision(!ops.mCollisionEnabled);
    AddConstraintMapping(*result);
    return result->GetHandle();
  }

  void ConstraintSystem::RemoveContact(ContactConstraint& toRemove) {
    mIslandGraph->Remove(toRemove);
    mPairToManifold.erase(mPairToManifold.find({toRemove.GetModelInstanceA(), toRemove.GetModelInstanceB()}));
    mContacts.Free(&toRemove);
  }

  static void OrderHandlePair(Handle& a, Handle& b ) {
    //Enforce arbitrary but consistent handle ordering
    if(a < b)
      std::swap(a, b);
  }

  bool ConstraintSystem::IsBlacklistPair(Handle a, Handle b) {
    OrderHandlePair(a, b);
    return mCollisionBlacklist.find({a, b}) != mCollisionBlacklist.end();
  }

  void ConstraintSystem::AddBlacklistPair(Handle a, Handle b) {
    OrderHandlePair(a, b);
    auto it = mCollisionBlacklist.find({a, b});
    //If we already have one, increment the reference count
    if(it != mCollisionBlacklist.end())
      it->second++;
    else
      mCollisionBlacklist[{a, b}] = 1;
  }

  void ConstraintSystem::RemoveBlacklistPair(Handle a, Handle b) {
    OrderHandlePair(a, b);
    auto it = mCollisionBlacklist.find({a, b});
    SyxAssertError(it != mCollisionBlacklist.end(), "Tried to remove blacklist pair that didn't exist");
    it->second--;
    if(!it->second)
      mCollisionBlacklist.erase(it);
  }

  Constraint* ConstraintSystem::GetConstraint(Handle handle) {
    auto it = mHandleToConstraint.find(handle);
    return it == mHandleToConstraint.end() ? nullptr : it->second;
  }

#define RemoveConstraintType(container, type)\
  RemoveConstraintMapping(*c);\
  container.Free(static_cast<type*>(c));

  void ConstraintSystem::RemoveConstraint(Handle handle) {
    Constraint* c = GetConstraint(handle);
    if(c) {
      switch(c->GetType()) {
        case ConstraintType::Distance: RemoveConstraintType(mDistances, DistanceConstraint); break;
        case ConstraintType::Spherical: RemoveConstraintType(mSphericals, SphericalConstraint); break;
        case ConstraintType::Weld: RemoveConstraintType(mWelds, WeldConstraint); break;
        case ConstraintType::Revolute: RemoveConstraintType(mRevolutes, RevoluteConstraint); break;
        default:
          SyxAssertError(false, "Tried to remove invalid constraint type");
      }
    }
  }

  void ConstraintSystem::AddConstraintMapping(Constraint& constraint) {
    Handle handle = constraint.GetHandle();
    SyxAssertError(mHandleToConstraint.find(handle) == mHandleToConstraint.end(), "Duplicate constraint handle addition");
    mHandleToConstraint[handle] = &constraint;
    mIslandGraph->Add(constraint);
    constraint.GetObjA()->AddConstraint(handle);
    constraint.GetObjB()->AddConstraint(handle);
    if(constraint.GetBlacklistCollision())
      AddBlacklistPair(constraint.GetObjA()->GetHandle(), constraint.GetObjB()->GetHandle());
  }

  void ConstraintSystem::RemoveConstraintMapping(Constraint& constraint) {
    if(constraint.GetBlacklistCollision())
      RemoveBlacklistPair(constraint.GetObjA()->GetHandle(), constraint.GetObjB()->GetHandle());
    Handle handle = constraint.GetHandle();
    auto it = mHandleToConstraint.find(handle);
    SyxAssertError(it != mHandleToConstraint.end(), "Tried to remmove constraint mapping that didn't exist");
    mHandleToConstraint.erase(it);
    mIslandGraph->Remove(constraint);
    constraint.GetObjA()->RemoveConstraint(handle);
    constraint.GetObjB()->RemoveConstraint(handle);
  }

  void ConstraintSystem::MoveConstraintMapping(Constraint& constraint) {
    SyxAssertError(mHandleToConstraint.find(constraint.GetHandle()) != mHandleToConstraint.end(), "Tried to move constraint mapping that didn't exist");
    mHandleToConstraint[constraint.GetHandle()] = &constraint;
  }

  void ConstraintSystem::ClearConstraintMappings() {
    mHandleToConstraint.clear();
  }
}