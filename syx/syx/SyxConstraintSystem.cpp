#include "Precompile.h"
#include "SyxConstraintSystem.h"

namespace Syx {
  int ConstraintSystem::sIterations = 10;
  float ConstraintSystem::sEarlyOutThreshold = 0.00001f;

  void ConstraintSystem::solve(float dt) {
    _createSolvers();
    for(size_t i = 0; i < mSolverCount; ++i) {
      IslandSolver& solver = mSolvers[i];
      solver.solve(dt);
      for(Constraint* toRemove : solver.getToRemove())
        _removeContact(*static_cast<ContactConstraint*>(toRemove));
      mIslandGraph->updateIslandState(solver.getIslandKey(), solver.getNewIslandState(), dt);
    }
  }

  void ConstraintSystem::sSolve(float dt) {
    _createSolvers();
    for(size_t i = 0; i < mSolverCount; ++i) {
      IslandSolver& solver = mSolvers[i];
      solver.sSolve(dt);
      for(Constraint* toRemove : solver.getToRemove())
        _removeContact(*static_cast<ContactConstraint*>(toRemove));
      mIslandGraph->updateIslandState(solver.getIslandKey(), solver.getNewIslandState(), dt);
    }
  }

  Manifold* ConstraintSystem::getManifold(PhysicsObject& objA, PhysicsObject& objB, ModelInstance& instA, ModelInstance& instB) {
    size_t oldSize = mPairToManifold.size();
    std::pair<Handle, Handle> pair = {instA.getHandle(), instB.getHandle()};
    Manifold*& foundManifold = mPairToManifold[pair];

    //Size didn't change, meaning nothing was inserted, so we found a manifold
    if(oldSize == mPairToManifold.size())
      return foundManifold;

    //There's going to be many more whitelisted pairs than blacklisted, so optimize for former
    if(_isBlacklistPair(objA.getHandle(), objB.getHandle())) {
      mPairToManifold.erase(pair);
      return nullptr;
    }

    //Didn't find a manifold. Make one and return it
    ContactConstraint* newConstraint = _createContact(&objA, &objB, instA.getHandle(), instB.getHandle());
    foundManifold = &newConstraint->mManifold;
    mIslandGraph->add(*newConstraint);
    return foundManifold;
  }

  void ConstraintSystem::updateManifolds(void) {
    for(auto it = mContacts.begin(); it != mContacts.end(); ++it)
      (*it).mManifold.update();
  }

  void ConstraintSystem::clear(void) {
    mContacts.clear();
    mDistances.clear();
    mRevolutes.clear();
    mSphericals.clear();
    mWelds.clear();
    mPairToManifold.clear();
    _clearConstraintMappings();
    mCollisionBlacklist.clear();
  }

  void ConstraintSystem::_createSolvers() {
    mSolverCount = mIslandGraph->islandCount();
    //Don't want to resize down because that would destruct solvers whose memory could be re-used later
    if(mSolvers.size() < mSolverCount) {
      mSolvers.resize(mSolverCount);
    }

    for(size_t i = 0; i < mSolverCount; ++i) {
      mIslandGraph->getIsland(i, mContents);
      mSolvers[i].set(mContents);
    }
  }

  ContactConstraint* ConstraintSystem::_createContact(PhysicsObject* objA, PhysicsObject* objB, Handle instA, Handle instB) {
    ContactConstraint* result = mContacts.push(ContactConstraint(objA, objB, mConstraintHandleGen.next(), instA, instB));
    SyxAssertError(mHandleToConstraint.find(result->getHandle()) == mHandleToConstraint.end(), "Created new constraint with duplicate handle");
    return result;
  }

  static void SetAnchors(const ConstraintOptions& ops, Constraint& constraint) {
    constraint.setLocalAnchor(ops.mAnchorA, ConstraintObj::A);
    constraint.setLocalAnchor(ops.mAnchorB, ConstraintObj::B);
  }

  Handle ConstraintSystem::addDistanceConstraint(const DistanceOps& ops) {
    DistanceConstraint* result = mDistances.push(DistanceConstraint(ops.mObjA, ops.mObjB, mConstraintHandleGen.next()));
    SetAnchors(ops, *result);
    result->setDistance(ops.mDistance);
    result->setBlacklistCollision(!ops.mCollisionEnabled);
    _addConstraintMapping(*result);
    return result->getHandle();
  }

  Handle ConstraintSystem::addSphericalConstraint(const SphericalOps& ops) {
    SphericalConstraint* result = mSphericals.push(SphericalConstraint(ops.mObjA, ops.mObjB, mConstraintHandleGen.next()));
    SetAnchors(ops, *result);
    result->setSwingFrame(ops.mSwingFrame);
    result->setSwingLimits(ops.mMaxSwingRadsX, ops.mMaxSwingRadsY);
    result->setTwistLimits(ops.mMinTwistRads, ops.mMaxTwistRads);
    result->setMaxAngularImpulse(ops.mMaxAngularImpulse);
    result->setBlacklistCollision(!ops.mCollisionEnabled);
    _addConstraintMapping(*result);
    return result->getHandle();
  }

  Handle ConstraintSystem::addWeldConstraint(const WeldOps& ops) {
    WeldConstraint* result = mWelds.push(WeldConstraint(ops.mObjA, ops.mObjB, mConstraintHandleGen.next()));
    result->lockRelativeTransform();
    result->setBlacklistCollision(!ops.mCollisionEnabled);
    _addConstraintMapping(*result);
    return result->getHandle();
  }

  Handle ConstraintSystem::addRevoluteConstraint(const RevoluteOps& ops) {
    RevoluteConstraint* result = mRevolutes.push(RevoluteConstraint(ops.mObjA, ops.mObjB, mConstraintHandleGen.next()));
    SetAnchors(ops, *result);
    result->setFreeLimits(ops.mMinRads, ops.mMaxRads);
    result->setLocalFreeAxis(ops.mFreeAxis);
    result->setMaxFreeImpulse(ops.mMaxFreeImpulse);
    result->setBlacklistCollision(!ops.mCollisionEnabled);
    _addConstraintMapping(*result);
    return result->getHandle();
  }

  void ConstraintSystem::_removeContact(ContactConstraint& toRemove) {
    mIslandGraph->remove(toRemove);
    mPairToManifold.erase(mPairToManifold.find({toRemove.getModelInstanceA(), toRemove.getModelInstanceB()}));
    mContacts.freeObj(&toRemove);
  }

  static void OrderHandlePair(Handle& a, Handle& b ) {
    //Enforce arbitrary but consistent handle ordering
    if(a < b)
      std::swap(a, b);
  }

  bool ConstraintSystem::_isBlacklistPair(Handle a, Handle b) {
    OrderHandlePair(a, b);
    return mCollisionBlacklist.find({a, b}) != mCollisionBlacklist.end();
  }

  void ConstraintSystem::_addBlacklistPair(Handle a, Handle b) {
    OrderHandlePair(a, b);
    auto it = mCollisionBlacklist.find({a, b});
    //If we already have one, increment the reference count
    if(it != mCollisionBlacklist.end())
      it->second++;
    else
      mCollisionBlacklist[{a, b}] = 1;
  }

  void ConstraintSystem::_removeBlacklistPair(Handle a, Handle b) {
    OrderHandlePair(a, b);
    auto it = mCollisionBlacklist.find({a, b});
    SyxAssertError(it != mCollisionBlacklist.end(), "Tried to remove blacklist pair that didn't exist");
    it->second--;
    if(!it->second)
      mCollisionBlacklist.erase(it);
  }

  Constraint* ConstraintSystem::getConstraint(Handle handle) {
    auto it = mHandleToConstraint.find(handle);
    return it == mHandleToConstraint.end() ? nullptr : it->second;
  }

#define RemoveConstraintType(container, type)\
  _removeConstraintMapping(constraint);\
  container.freeObj(static_cast<type*>(&constraint));

  void ConstraintSystem::removeConstraint(Handle handle) {
    if(Constraint* c = getConstraint(handle)) {
      removeConstraint(*c);
    }
  }

  void ConstraintSystem::removeConstraint(Constraint& constraint) {
      constraint.getObjA()->setAsleep(false);
      constraint.getObjB()->setAsleep(false);
      switch(constraint.getType()) {
        case ConstraintType::Contact: _removeContact(static_cast<ContactConstraint&>(constraint)); break;
        case ConstraintType::Distance: RemoveConstraintType(mDistances, DistanceConstraint); break;
        case ConstraintType::Spherical: RemoveConstraintType(mSphericals, SphericalConstraint); break;
        case ConstraintType::Weld: RemoveConstraintType(mWelds, WeldConstraint); break;
        case ConstraintType::Revolute: RemoveConstraintType(mRevolutes, RevoluteConstraint); break;
        default:
          SyxAssertError(false, "Tried to remove invalid constraint type");
      }
  }

  void ConstraintSystem::_addConstraintMapping(Constraint& constraint) {
    Handle handle = constraint.getHandle();
    SyxAssertError(mHandleToConstraint.find(handle) == mHandleToConstraint.end(), "Duplicate constraint handle addition");
    mHandleToConstraint[handle] = &constraint;
    mIslandGraph->add(constraint);
    constraint.getObjA()->addConstraint(handle);
    constraint.getObjB()->addConstraint(handle);
    if(constraint.getBlacklistCollision())
      _addBlacklistPair(constraint.getObjA()->getHandle(), constraint.getObjB()->getHandle());
  }

  void ConstraintSystem::_removeConstraintMapping(Constraint& constraint) {
    if(constraint.getBlacklistCollision())
      _removeBlacklistPair(constraint.getObjA()->getHandle(), constraint.getObjB()->getHandle());
    Handle handle = constraint.getHandle();
    auto it = mHandleToConstraint.find(handle);
    SyxAssertError(it != mHandleToConstraint.end(), "Tried to remmove constraint mapping that didn't exist");
    mHandleToConstraint.erase(it);
    mIslandGraph->remove(constraint);
    constraint.getObjA()->removeConstraint(handle);
    constraint.getObjB()->removeConstraint(handle);
  }

  void ConstraintSystem::_moveConstraintMapping(Constraint& constraint) {
    SyxAssertError(mHandleToConstraint.find(constraint.getHandle()) != mHandleToConstraint.end(), "Tried to move constraint mapping that didn't exist");
    mHandleToConstraint[constraint.getHandle()] = &constraint;
  }

  void ConstraintSystem::_clearConstraintMappings() {
    mHandleToConstraint.clear();
  }
}