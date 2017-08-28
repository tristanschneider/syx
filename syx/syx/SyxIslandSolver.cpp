#include "Precompile.h"
#include "SyxIslandSolver.h"
#include "SyxConstraintSystem.h"

namespace Syx {
  void IslandSolver::_clearLocalConstraints() {
    mWelds.clear();
    mContacts.clear();
    mRevolutes.clear();
    mDistances.clear();
    mSphericals.clear();
  }

  void IslandSolver::set(const IslandContents& island) {
    _clearLocalConstraints();
    mObjects.clear();
    mToRemove.clear();
    mObjHandleToIndex.clear();

    mIslandKey = island.mIslandKey;
    mNewIslandState = SleepState::Inactive;
    mCurIslandState = island.mSleepState;

    //If we're asleep and it's not news, don't build anything, this island is asleep
    if(island.mSleepState == SleepState::Inactive)
      return;

    //Maximum number of objects this could be so we can hold pointers without worrying about a resize
    mObjects.reserve(island.mConstraints.size()*2);
    for(Constraint* constraint : island.mConstraints) {
      PhysicsObject* objA = constraint->getObjA();
      PhysicsObject* objB = constraint->getObjB();
      //Island is inactive if all objects in it are inactive
      if(mNewIslandState == SleepState::Inactive && (!objA->isInactive() || !objB->isInactive()))
        mNewIslandState = SleepState::Active;

      //Update sleep state while we have the objects
      switch(island.mSleepState) {
        case SleepState::Asleep:
          objA->setAsleep(true);
          objB->setAsleep(true);
          continue;

        case SleepState::Awake:
          objA->setAsleep(false);
          objB->setAsleep(false);
          break;

        case SleepState::Active:
          SyxAssertError(objA->isStatic() || !objA->getAsleep(), "Object should be awake");
          break;
      }
      size_t indexA = _getObjectIndex(*objA);
      size_t indexB = _getObjectIndex(*objB);

      _pushLocalConstraint(*constraint, indexA, indexB);
    }
  }

#define PushConstraintType(constraintType, container) {\
    constraintType local;\
    local.set(mObjects[indexA], mObjects[indexB], constraint);\
    container.push_back(local);\
  }

  void IslandSolver::_pushLocalConstraint(Constraint& constraint, size_t indexA, size_t indexB) {
    switch(constraint.getType()) {
      case ConstraintType::Contact: {
        ContactConstraint& contact = static_cast<ContactConstraint&>(constraint);
        LocalContactConstraint local(contact);
        local.set(mObjects[indexA], mObjects[indexB], constraint);
        mContacts.push_back(local);
        break;
      }
      case ConstraintType::Distance: PushConstraintType(LocalDistanceConstraint, mDistances); break;
      case ConstraintType::Spherical: PushConstraintType(LocalSphericalConstraint, mSphericals); break;
      case ConstraintType::Weld: PushConstraintType(LocalWeldConstraint, mWelds); break;
      case ConstraintType::Revolute: PushConstraintType(LocalRevoluteConstraint, mRevolutes); break;
      default: SyxAssertError(false, "Invalid constraint type");
    }
  }

  size_t IslandSolver::_getObjectIndex(PhysicsObject& obj) {
    auto it = mObjHandleToIndex.find(obj.getHandle());
    if(it != mObjHandleToIndex.end()) {
      return it->second;
    }

    size_t newIndex = mObjects.size();
    mObjHandleToIndex[obj.getHandle()] = newIndex;
    mObjects.push_back(LocalObject(obj));
    return newIndex;
  }

  template <typename Container>
  static void solveContainer(Container& container, float& maxImpulse) {
    for(auto& constraint : container) {
      maxImpulse = std::max(maxImpulse, constraint.solve());
    }
  }

  void IslandSolver::solve(float dt) {
    if(mCurIslandState == SleepState::Inactive)
      return;
    preSolve(dt);
    for(int i = 0; i < ConstraintSystem::sIterations; ++i) {
      float maxImpulse = 0.0f;
      solveContainer(mSphericals, maxImpulse);
      solveContainer(mRevolutes, maxImpulse);
      solveContainer(mDistances, maxImpulse);
      solveContainer(mWelds, maxImpulse);
      solveContainer(mContacts, maxImpulse);

      if(maxImpulse < ConstraintSystem::sEarlyOutThreshold)
        break;
    }
    postSolve();
    storeObjects();
  }

  template <typename Container>
  static void sSolveContainer(Container& container, float& maxImpulse) {
    for(auto& constraint : container) {
      maxImpulse = std::max(maxImpulse, constraint.sSolve());
    }
  }

  void IslandSolver::sSolve(float dt) {
    if(mCurIslandState == SleepState::Inactive)
      return;
    preSolve(dt);
    for(int i = 0; i < ConstraintSystem::sIterations; ++i) {
      float maxImpulse = 0.0f;
      sSolveContainer(mSphericals, maxImpulse);
      sSolveContainer(mRevolutes, maxImpulse);
      sSolveContainer(mDistances, maxImpulse);
      sSolveContainer(mWelds, maxImpulse);
      sSolveContainer(mContacts, maxImpulse);

      if(maxImpulse < ConstraintSystem::sEarlyOutThreshold)
        break;
    }
    postSolve();
    storeObjects();
  }

  void IslandSolver::storeObjects() {
    for(LocalObject& obj : mObjects) {
      //Static objects won't change, so don't write them, also, multiple islands share these, so when multi-threaded, writes to these could be bad
      PhysicsObject* owner = obj.mOwner;
      if(Rigidbody* rb = owner->getRigidbody()) {
        rb->mLinVel = obj.mLinVel;
        rb->mAngVel = obj.mAngVel;
      }
      //No position solving yet, so don't need to store that
    }
  }

  template <typename Container>
  static void _preSolveContainer(Container& container, std::vector<Constraint*>& toRemove, bool shouldDraw) {
    //Initial loop through constraints sets them up and removes old ones
    for(size_t i = 0; i < container.size();) {
      auto& localConstraint = container[i];
      Constraint* owner = localConstraint.getOwner();
      if(owner->shouldRemove()) {
        toRemove.push_back(owner);
        swapRemove(container, i);
        continue;
      }

      localConstraint.firstIteration();
      if(shouldDraw)
        localConstraint.draw();
      ++i;
    }
  }

  void IslandSolver::preSolve(float dt) {
    Constraint::sDT = dt;
    bool shouldDrawContacts = (Interface::getOptions().mDebugFlags & SyxOptions::DrawManifolds) != 0;
    bool shouldDrawJoints = (Interface::getOptions().mDebugFlags & SyxOptions::DrawJoints) != 0;
    mToRemove.clear();

    _preSolveContainer(mSphericals, mToRemove, shouldDrawJoints);
    _preSolveContainer(mRevolutes, mToRemove, shouldDrawJoints);
    _preSolveContainer(mDistances, mToRemove, shouldDrawJoints);
    _preSolveContainer(mWelds, mToRemove, shouldDrawJoints);
    _preSolveContainer(mContacts, mToRemove, shouldDrawContacts);
  }

  template <typename Container>
  static void _postSolveContainer(Container& container) {
    for(auto& constraint : container)
      constraint.lastIteration();
  }

  void IslandSolver::postSolve() {
    _postSolveContainer(mSphericals);
    _postSolveContainer(mRevolutes);
    _postSolveContainer(mDistances);
    _postSolveContainer(mWelds);
    _postSolveContainer(mContacts);
  }

  const std::vector<Constraint*>& IslandSolver::getToRemove() {
    return mToRemove;
  }

  IndexableKey IslandSolver::getIslandKey() {
    return mIslandKey;
  }

  SleepState IslandSolver::getNewIslandState() {
    return mNewIslandState;
  }
}