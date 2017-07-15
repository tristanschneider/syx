#include "Precompile.h"
#include "SyxIslandSolver.h"
#include "SyxConstraintSystem.h"

namespace Syx {
  void IslandSolver::ClearLocalConstraints() {
    mWelds.clear();
    mContacts.clear();
    mRevolutes.clear();
    mDistances.clear();
    mSphericals.clear();
  }

  void IslandSolver::Set(const IslandContents& island) {
    ClearLocalConstraints();
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
      PhysicsObject* objA = constraint->GetObjA();
      PhysicsObject* objB = constraint->GetObjB();
      //Island is inactive if all objects in it are inactive
      if(mNewIslandState == SleepState::Inactive && (!objA->IsInactive() || !objB->IsInactive()))
        mNewIslandState = SleepState::Active;

      //Update sleep state while we have the objects
      switch(island.mSleepState) {
        case SleepState::Asleep:
          objA->SetAsleep(true);
          objB->SetAsleep(true);
          continue;

        case SleepState::Awake:
          objA->SetAsleep(false);
          objB->SetAsleep(false);
          break;

        case SleepState::Active:
          SyxAssertError(objA->IsStatic() || !objA->GetAsleep(), "Object should be awake");
          break;
      }
      size_t indexA = GetObjectIndex(*objA);
      size_t indexB = GetObjectIndex(*objB);

      PushLocalConstraint(*constraint, indexA, indexB);
    }
  }

#define PushConstraintType(constraintType, container) {\
    constraintType local;\
    local.Set(mObjects[indexA], mObjects[indexB], constraint);\
    container.push_back(local);\
  }

  void IslandSolver::PushLocalConstraint(Constraint& constraint, size_t indexA, size_t indexB) {
    switch(constraint.GetType()) {
      case ConstraintType::Contact: {
        ContactConstraint& contact = static_cast<ContactConstraint&>(constraint);
        LocalContactConstraint local(contact);
        local.Set(mObjects[indexA], mObjects[indexB], constraint);
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

  size_t IslandSolver::GetObjectIndex(PhysicsObject& obj) {
    auto it = mObjHandleToIndex.find(obj.GetHandle());
    if(it != mObjHandleToIndex.end()) {
      return it->second;
    }

    size_t newIndex = mObjects.size();
    mObjHandleToIndex[obj.GetHandle()] = newIndex;
    mObjects.push_back(LocalObject(obj));
    return newIndex;
  }

  template <typename Container>
  static void SolveContainer(Container& container, float& maxImpulse) {
    for(auto& constraint : container) {
      maxImpulse = std::max(maxImpulse, constraint.Solve());
    }
  }

  void IslandSolver::Solve(float dt) {
    if(mCurIslandState == SleepState::Inactive)
      return;
    PreSolve(dt);
    for(int i = 0; i < ConstraintSystem::sIterations; ++i) {
      float maxImpulse = 0.0f;
      SolveContainer(mSphericals, maxImpulse);
      SolveContainer(mRevolutes, maxImpulse);
      SolveContainer(mDistances, maxImpulse);
      SolveContainer(mWelds, maxImpulse);
      SolveContainer(mContacts, maxImpulse);

      if(maxImpulse < ConstraintSystem::sEarlyOutThreshold)
        break;
    }
    PostSolve();
    StoreObjects();
  }

  template <typename Container>
  static void SSolveContainer(Container& container, float& maxImpulse) {
    for(auto& constraint : container) {
      maxImpulse = std::max(maxImpulse, constraint.SSolve());
    }
  }

  void IslandSolver::SSolve(float dt) {
    if(mCurIslandState == SleepState::Inactive)
      return;
    PreSolve(dt);
    for(int i = 0; i < ConstraintSystem::sIterations; ++i) {
      float maxImpulse = 0.0f;
      SSolveContainer(mSphericals, maxImpulse);
      SolveContainer(mRevolutes, maxImpulse);
      SSolveContainer(mDistances, maxImpulse);
      SSolveContainer(mWelds, maxImpulse);
      SSolveContainer(mContacts, maxImpulse);

      if(maxImpulse < ConstraintSystem::sEarlyOutThreshold)
        break;
    }
    PostSolve();
    StoreObjects();
  }

  void IslandSolver::StoreObjects() {
    for(LocalObject& obj : mObjects) {
      //Static objects won't change, so don't write them, also, multiple islands share these, so when multi-threaded, writes to these could be bad
      PhysicsObject* owner = obj.mOwner;
      if(Rigidbody* rb = owner->GetRigidbody()) {
        rb->mLinVel = obj.mLinVel;
        rb->mAngVel = obj.mAngVel;
      }
      //No position solving yet, so don't need to store that
    }
  }

  template <typename Container>
  static void PreSolveContainer(Container& container, std::vector<Constraint*>& toRemove, bool shouldDraw) {
    //Initial loop through constraints sets them up and removes old ones
    for(size_t i = 0; i < container.size();) {
      auto& localConstraint = container[i];
      Constraint* owner = localConstraint.GetOwner();
      if(owner->ShouldRemove()) {
        toRemove.push_back(owner);
        SwapRemove(container, i);
        continue;
      }

      localConstraint.FirstIteration();
      if(shouldDraw)
        localConstraint.Draw();
      ++i;
    }
  }

  void IslandSolver::PreSolve(float dt) {
    Constraint::sDT = dt;
    bool shouldDrawContacts = (Interface::GetOptions().mDebugFlags & SyxOptions::DrawManifolds) != 0;
    bool shouldDrawJoints = (Interface::GetOptions().mDebugFlags & SyxOptions::DrawJoints) != 0;
    mToRemove.clear();

    PreSolveContainer(mSphericals, mToRemove, shouldDrawJoints);
    PreSolveContainer(mRevolutes, mToRemove, shouldDrawJoints);
    PreSolveContainer(mDistances, mToRemove, shouldDrawJoints);
    PreSolveContainer(mWelds, mToRemove, shouldDrawJoints);
    PreSolveContainer(mContacts, mToRemove, shouldDrawContacts);
  }

  template <typename Container>
  void PostSolveContainer(Container& container) {
    for(auto& constraint : container)
      constraint.LastIteration();
  }

  void IslandSolver::PostSolve() {
    PostSolveContainer(mSphericals);
    PostSolveContainer(mRevolutes);
    PostSolveContainer(mDistances);
    PostSolveContainer(mWelds);
    PostSolveContainer(mContacts);
  }

  const std::vector<Constraint*>& IslandSolver::GetToRemove() {
    return mToRemove;
  }

  IndexableKey IslandSolver::GetIslandKey() {
    return mIslandKey;
  }

  SleepState IslandSolver::GetNewIslandState() {
    return mNewIslandState;
  }
}