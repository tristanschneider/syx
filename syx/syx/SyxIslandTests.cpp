#include "Precompile.h"
#include "SyxIslandGraph.h"
#include "SyxConstraint.h"
#include "SyxPhysicsObject.h"
#include "SyxTestHelpers.h"

namespace Syx {
  namespace {
    std::vector<bool(*)()> islandTests;

    bool Contains(const IslandContents& contents, const Constraint& constraint) {
      for(const Constraint* c : contents.mConstraints)
        if(c->getHandle() == constraint.getHandle())
          return true;
      return false;
    }

    bool Match(const IslandContents& contents, const std::vector<Constraint*> constraints) {
      if(contents.mConstraints.size() != constraints.size())
        return false;
      for(Constraint* constraint : constraints)
        if(!Contains(contents, *constraint))
          return false;
      return true;
    }

    bool Match(const IslandContents& contentsA, const IslandContents& contentsB, const std::vector<Constraint*> correctA, const std::vector<Constraint*> correctB) {
      //We don't enforce an ordering, so if any works 
      bool aFound = Match(contentsA, correctA) || Match(contentsB, correctA);
      bool bFound = Match(contentsA, correctB) || Match(contentsB, correctB);
      return aFound && bFound;
    }
  }

  TEST_FUNC(islandTests, testIslandAdd) {
    TEST_FAILED = false;
    Handle id = 0;
    PhysicsObject a(id++);
    PhysicsObject b(id++);
    PhysicsObject c(id++);
    PhysicsObject d(id++);
    PhysicsObject e(id++);
    PhysicsObject f(id++);
    PhysicsObject g(id++);

    PhysicsObject staticA(id++);
    PhysicsObject staticB(id++);
    staticA.setRigidbodyEnabled(false);
    staticB.setRigidbodyEnabled(false);

    Constraint ab(ConstraintType::Invalid, &a, &b, id++);
    Constraint cd(ConstraintType::Invalid, &c, &d, id++);
    Constraint ae(ConstraintType::Invalid, &a, &e, id++);
    Constraint bc(ConstraintType::Invalid, &b, &c, id++);
    Constraint esa(ConstraintType::Invalid, &e, &staticA, id++);
    Constraint fsb(ConstraintType::Invalid, &f, &staticB, id++);
    Constraint fg(ConstraintType::Invalid, &f, &g, id++);
    Constraint ef(ConstraintType::Invalid, &e, &f, id++);
    Constraint cf(ConstraintType::Invalid, &c, &f, id++);
    Constraint asa(ConstraintType::Invalid, &a, &staticA, id++);
    Constraint bsa(ConstraintType::Invalid, &b, &staticA, id++);
    Constraint csa(ConstraintType::Invalid, &c, &staticA, id++);
    Constraint dsa(ConstraintType::Invalid, &d, &staticA, id++);
    IslandContents contentsA, contentsB;
    IslandGraph graph;

    //Create single island
    graph.add(ab);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(contentsA.mConstraints.size() == 1);
    checkResult(Match(contentsA, {&ab}));
    checkResult(graph.validate());

    //Create second island
    graph.add(cd);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab}, {&cd}));
    checkResult(graph.validate());

    //Add to existing island
    graph.add(ae);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ae, &ab}, {&cd}));
    checkResult(graph.validate());

    //Merge size 2 with size 1 island
    graph.add(bc);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(Match(contentsA, {&ab, &cd, &ae, &bc}));
    checkResult(graph.validate());

    //Start island with static node
    graph.add(fsb);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &cd, &ae, &bc}, {&fsb}));
    checkResult(graph.validate());

    //Add static node to existing island
    graph.add(esa);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &cd, &ae, &bc, &esa}, {&fsb}));
    checkResult(graph.validate());

    //Merge two islands with multiple nodes and static objects
    graph.add(fg);
    checkResult(graph.islandCount() == 2);
    graph.add(ef);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(Match(contentsA, {&ab, &cd, &ae, &bc, &esa, &fsb, &fg, &ef}));
    checkResult(graph.validate());

    //Test clear
    graph.clear();
    checkResult(graph.islandCount() == 0);
    checkResult(graph.validate());

    //Test having separate islands both containing the same static node
    Constraint fsa(ConstraintType::Invalid, &f, &staticA, id++);
    graph.add(ab);
    graph.add(ae);
    graph.add(esa);

    graph.add(cf);
    graph.add(fg);
    graph.add(fsa);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &ae, &esa}, {&cf, &fg, &fsa}));
    checkResult(graph.validate());

    graph.clear();

    //Test count validity when adding two constraints to the same static object
    graph.add(ab);
    graph.add(asa);
    graph.add(bsa);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(Match(contentsA, {&ab, &asa, &bsa}));
    checkResult(graph.validate());

    graph.clear();

    //Test static edge count validity when merging islands with multiple static edges
    graph.add(ab);
    graph.add(asa);
    graph.add(bsa);
    graph.add(cd);
    graph.add(csa);
    graph.add(dsa);
    graph.add(bc);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(Match(contentsA, {&ab, &asa, &bsa, &cd, &csa, &dsa, &bc}));
    checkResult(graph.validate());

    return TEST_FAILED;
  }

  TEST_FUNC(islandTests, testIslandRemoveConstraint) {
    TEST_FAILED = false;
    Handle id = 0;
    PhysicsObject a(id++);
    PhysicsObject b(id++);
    PhysicsObject c(id++);
    PhysicsObject d(id++);
    PhysicsObject e(id++);
    PhysicsObject f(id++);
    PhysicsObject g(id++);

    PhysicsObject staticA(id++);
    staticA.setRigidbodyEnabled(false);

    IslandContents contentsA, contentsB;
    IslandGraph graph;

    Constraint ab(ConstraintType::Invalid, &a, &b, id++);
    Constraint bc(ConstraintType::Invalid, &b, &c, id++);
    Constraint cd(ConstraintType::Invalid, &c, &d, id++);
    Constraint de(ConstraintType::Invalid, &d, &e, id++);
    Constraint ef(ConstraintType::Invalid, &e, &f, id++);
    Constraint ac(ConstraintType::Invalid, &a, &c, id++);
    Constraint bd(ConstraintType::Invalid, &b, &d, id++);
    Constraint asa(ConstraintType::Invalid, &a, &staticA, id++);
    Constraint bsa(ConstraintType::Invalid, &b, &staticA, id++);
    Constraint csa(ConstraintType::Invalid, &c, &staticA, id++);
    Constraint dsa(ConstraintType::Invalid, &d, &staticA, id++);

    //Removal of size 2 island
    graph.add(ab);
    graph.remove(ab);
    checkResult(graph.islandCount() == 0);
    checkResult(graph.validate());

    //Removal of size 3 island and leaf removal and root removal
    graph.add(ab);
    graph.add(bc);
    graph.remove(ab);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(Match(contentsA, {&bc}));
    graph.remove(bc);
    checkResult(graph.islandCount() == 0);
    checkResult(graph.validate());

    //Split with size 2 island and size 4 island
    graph.add(ab);
    graph.add(bc);
    graph.add(cd);
    graph.add(de);
    graph.add(ef);
    graph.remove(de);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &bc, &cd}, {&ef}));
    checkResult(graph.validate());

    graph.clear();

    //Split with size 3 size 3
    graph.add(ab);
    graph.add(bc);
    graph.add(cd);
    graph.add(de);
    graph.add(ef);
    graph.remove(cd);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &bc}, {&de, &ef}));
    checkResult(graph.validate());

    graph.clear();

    //Test count validity when adding two constraints to the same static object
    graph.add(ab);
    graph.add(asa);
    graph.add(bsa);
    graph.remove(bsa);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(Match(contentsA, {&ab, &asa}));
    checkResult(graph.validate());

    graph.clear();

    //Test leaf removal where both sides share the same static object
    graph.add(ab);
    graph.add(bc);
    graph.add(asa);
    graph.add(csa);
    graph.remove(bc);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &asa}, {&csa}));
    checkResult(graph.validate());

    graph.clear();

    //Test split where both sides share the same static object
    graph.add(ab);
    graph.add(bc);
    graph.add(cd);
    graph.add(asa);
    graph.add(csa);
    graph.remove(bc);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &asa}, {&cd, &csa}));
    checkResult(graph.validate());

    graph.clear();

    //Test split with more static node references on the left
    graph.add(ab);
    graph.add(bc);
    graph.add(cd);
    graph.add(asa);
    graph.add(bsa);
    graph.add(csa);
    graph.remove(bc);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &asa, &bsa}, {&cd, &csa}));
    checkResult(graph.validate());
    //Make sure reference count for static node works
    graph.remove(bsa);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &asa}, {&cd, &csa}));
    checkResult(graph.validate());

    graph.clear();

    //Test split with more static node references on the right
    graph.add(ab);
    graph.add(bc);
    graph.add(cd);
    graph.add(asa);
    graph.add(csa);
    graph.add(dsa);
    graph.remove(bc);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &asa}, {&cd, &csa, &dsa}));
    checkResult(graph.validate());
    //Make sure reference count for static node works
    graph.remove(csa);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab, &asa}, {&cd, &dsa}));
    checkResult(graph.validate());

    graph.clear();

    //Removal with edges on both sides that DOESN'T lead to a split
    graph.add(ab);
    graph.add(ac);
    graph.add(cd);
    graph.add(bd);
    graph.add(bc);
    graph.add(asa);
    graph.add(bsa);
    graph.remove(bc);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(Match(contentsA, {&ab, &ac, &cd, &bd, &asa, &bsa}));
    checkResult(graph.validate());

    return TEST_FAILED;
  }

  TEST_FUNC(islandTests, testIslandRemoveObject) {
    TEST_FAILED = false;
    Handle id = 0;
    PhysicsObject a(id++);
    PhysicsObject b(id++);
    PhysicsObject c(id++);
    PhysicsObject d(id++);
    PhysicsObject e(id++);
    PhysicsObject f(id++);

    PhysicsObject staticA(id++);
    PhysicsObject staticB(id++);
    staticA.setRigidbodyEnabled(false);
    staticB.setRigidbodyEnabled(false);

    IslandContents contentsA, contentsB;
    IslandGraph graph;

    Constraint ab(ConstraintType::Invalid, &a, &b, id++);
    Constraint bc(ConstraintType::Invalid, &b, &c, id++);
    Constraint cd(ConstraintType::Invalid, &c, &d, id++);
    Constraint de(ConstraintType::Invalid, &d, &e, id++);

    Constraint ac(ConstraintType::Invalid, &a, &c, id++);
    Constraint ad(ConstraintType::Invalid, &a, &d, id++);
    Constraint ae(ConstraintType::Invalid, &a, &e, id++);
    Constraint af(ConstraintType::Invalid, &a, &f, id++);

    //Removal of object in pair
    graph.add(ab);
    graph.remove(a);
    checkResult(graph.islandCount() == 0);
    checkResult(graph.validate());

    //Removal in middle of chain resulting in size 2 islands on both sides
    graph.add(ab);
    graph.add(bc);
    graph.add(cd);
    graph.add(de);
    graph.remove(c);
    checkResult(graph.islandCount() == 2);
    graph.getIsland(0, contentsA);
    graph.getIsland(1, contentsB);
    checkResult(Match(contentsA, contentsB, {&ab}, {&de}));
    checkResult(graph.validate());

    graph.clear();

    //Remove center of wheel with a size two island and a bunch of lone nodes
    graph.add(ab);
    graph.add(ac);
    graph.add(bc);
    graph.add(ad);
    graph.add(ae);
    graph.add(af);
    graph.remove(a);
    checkResult(graph.islandCount() == 1);
    graph.getIsland(0, contentsA);
    checkResult(Match(contentsA, {&bc}));
    checkResult(graph.validate());

    return TEST_FAILED;
  }

  TEST_FUNC(islandTests, island_remove_static_object_from_pair_island_is_removed) {
    Handle id = 0;
    PhysicsObject staticObj(id++);
    staticObj.setRigidbodyEnabled(false);
    PhysicsObject dynamicObj(id++);
    Constraint c(ConstraintType::Invalid, &staticObj, &dynamicObj, id++);
    IslandGraph graph;
    IslandContents contents;

    graph.add(c);
    graph.remove(staticObj);

    assert(graph.islandCount() == 0 && "Island should be removed");
    return false;
  }

  TEST_FUNC(islandTests, testIslandSleep) {
    TEST_FAILED = false;
    Handle id = 0;
    PhysicsObject a(id++);
    PhysicsObject b(id++);
    PhysicsObject c(id++);
    PhysicsObject d(id++);

    PhysicsObject staticA(id++);
    staticA.setRigidbodyEnabled(false);

    IslandContents contents;
    IslandGraph graph;

    Constraint ab(ConstraintType::Invalid, &a, &b, id++);
    Constraint bc(ConstraintType::Invalid, &b, &c, id++);
    Constraint cd(ConstraintType::Invalid, &c, &d, id++);
    Constraint saa(ConstraintType::Invalid, &staticA, &a, id++);
    Constraint sad(ConstraintType::Invalid, &staticA, &d, id++);

    //Test basic sleep transition on single island without composure changes.
    graph.add(ab);
    graph.getIsland(0, contents);
    checkResult(contents.mSleepState == SleepState::Awake);

    //Inactive not enough to sleep
    graph.updateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*0.5f);
    graph.getIsland(0, contents);
    //Not enough to sleep yet, but should have switched from awake to active now that Awake is not a new state anymore
    checkResult(contents.mSleepState == SleepState::Active);

    //Inactive enough to sleep
    graph.updateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep);
    graph.getIsland(0, contents);
    checkResult(contents.mSleepState == SleepState::Asleep);

    //Inactive when sleeping
    graph.updateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep);
    graph.getIsland(0, contents);
    checkResult(contents.mSleepState == SleepState::Inactive);

    //Wake from sleep
    graph.updateIslandState(contents.mIslandKey, SleepState::Active, 0.0f);
    graph.getIsland(0, contents);
    checkResult(contents.mSleepState == SleepState::Awake);

    //Make sure additions lead to wake up
    graph.updateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);
    graph.add(bc);
    graph.add(cd);
    graph.add(saa);
    graph.add(sad);
    graph.getIsland(0, contents);
    checkResult(contents.mSleepState == SleepState::Awake);

    //Make sure removals lead to wake up
    graph.updateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);
    graph.remove(bc);
    graph.getIsland(0, contents);
    checkResult(contents.mSleepState == SleepState::Awake);
    graph.getIsland(1, contents);
    checkResult(contents.mSleepState == SleepState::Awake);

    //Test force wake non-static node
    graph.add(bc);
    graph.getIsland(0, contents);
    graph.updateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);;
    graph.wakeIsland(b);
    graph.getIsland(0, contents);
    checkResult(contents.mSleepState == SleepState::Awake);

    //Test force wake static node
    graph.remove(bc);
    //Put both islands to sleep
    graph.getIsland(0, contents);
    graph.updateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);
    graph.getIsland(1, contents);
    graph.updateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);
    graph.wakeIsland(staticA);
    graph.getIsland(0, contents);
    checkResult(contents.mSleepState == SleepState::Awake);
    graph.getIsland(1, contents);
    checkResult(contents.mSleepState == SleepState::Awake);

    return TEST_FAILED;
  }

  bool testIslandAll() {
    bool failed = false;
    for(auto func : islandTests)
      failed = func() || failed;
    return failed;
  }
}