#include "Precompile.h"
#include "SyxIslandGraph.h"
#include "SyxConstraint.h"
#include "SyxPhysicsObject.h"
#include "SyxTestHelpers.h"

namespace Syx {
  bool Contains(const IslandContents& contents, const Constraint& constraint) {
    for(const Constraint* c : contents.mConstraints)
      if(c->GetHandle() == constraint.GetHandle())
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

  bool TestIslandAdd() {
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
    staticA.SetRigidbodyEnabled(false);
    staticB.SetRigidbodyEnabled(false);

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
    graph.Add(ab);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(contentsA.mConstraints.size() == 1);
    CheckResult(Match(contentsA, {&ab}));
    CheckResult(graph.Validate());

    //Create second island
    graph.Add(cd);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab}, {&cd}));
    CheckResult(graph.Validate());

    //Add to existing island
    graph.Add(ae);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ae, &ab}, {&cd}));
    CheckResult(graph.Validate());

    //Merge size 2 with size 1 island
    graph.Add(bc);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(Match(contentsA, {&ab, &cd, &ae, &bc}));
    CheckResult(graph.Validate());

    //Start island with static node
    graph.Add(fsb);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &cd, &ae, &bc}, {&fsb}));
    CheckResult(graph.Validate());

    //Add static node to existing island
    graph.Add(esa);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &cd, &ae, &bc, &esa}, {&fsb}));
    CheckResult(graph.Validate());

    //Merge two islands with multiple nodes and static objects
    graph.Add(fg);
    CheckResult(graph.IslandCount() == 2);
    graph.Add(ef);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(Match(contentsA, {&ab, &cd, &ae, &bc, &esa, &fsb, &fg, &ef}));
    CheckResult(graph.Validate());

    //Test clear
    graph.Clear();
    CheckResult(graph.IslandCount() == 0);
    CheckResult(graph.Validate());

    //Test having separate islands both containing the same static node
    Constraint fsa(ConstraintType::Invalid, &f, &staticA, id++);
    graph.Add(ab);
    graph.Add(ae);
    graph.Add(esa);

    graph.Add(cf);
    graph.Add(fg);
    graph.Add(fsa);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &ae, &esa}, {&cf, &fg, &fsa}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Test count validity when adding two constraints to the same static object
    graph.Add(ab);
    graph.Add(asa);
    graph.Add(bsa);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(Match(contentsA, {&ab, &asa, &bsa}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Test static edge count validity when merging islands with multiple static edges
    graph.Add(ab);
    graph.Add(asa);
    graph.Add(bsa);
    graph.Add(cd);
    graph.Add(csa);
    graph.Add(dsa);
    graph.Add(bc);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(Match(contentsA, {&ab, &asa, &bsa, &cd, &csa, &dsa, &bc}));
    CheckResult(graph.Validate());

    return TEST_FAILED;
  }

  bool TestIslandRemoveConstraint() {
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
    staticA.SetRigidbodyEnabled(false);

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
    graph.Add(ab);
    graph.Remove(ab);
    CheckResult(graph.IslandCount() == 0);
    CheckResult(graph.Validate());

    //Removal of size 3 island and leaf removal and root removal
    graph.Add(ab);
    graph.Add(bc);
    graph.Remove(ab);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(Match(contentsA, {&bc}));
    graph.Remove(bc);
    CheckResult(graph.IslandCount() == 0);
    CheckResult(graph.Validate());

    //Split with size 2 island and size 4 island
    graph.Add(ab);
    graph.Add(bc);
    graph.Add(cd);
    graph.Add(de);
    graph.Add(ef);
    graph.Remove(de);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &bc, &cd}, {&ef}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Split with size 3 size 3
    graph.Add(ab);
    graph.Add(bc);
    graph.Add(cd);
    graph.Add(de);
    graph.Add(ef);
    graph.Remove(cd);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &bc}, {&de, &ef}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Test count validity when adding two constraints to the same static object
    graph.Add(ab);
    graph.Add(asa);
    graph.Add(bsa);
    graph.Remove(bsa);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(Match(contentsA, {&ab, &asa}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Test leaf removal where both sides share the same static object
    graph.Add(ab);
    graph.Add(bc);
    graph.Add(asa);
    graph.Add(csa);
    graph.Remove(bc);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &asa}, {&csa}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Test split where both sides share the same static object
    graph.Add(ab);
    graph.Add(bc);
    graph.Add(cd);
    graph.Add(asa);
    graph.Add(csa);
    graph.Remove(bc);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &asa}, {&cd, &csa}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Test split with more static node references on the left
    graph.Add(ab);
    graph.Add(bc);
    graph.Add(cd);
    graph.Add(asa);
    graph.Add(bsa);
    graph.Add(csa);
    graph.Remove(bc);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &asa, &bsa}, {&cd, &csa}));
    CheckResult(graph.Validate());
    //Make sure reference count for static node works
    graph.Remove(bsa);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &asa}, {&cd, &csa}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Test split with more static node references on the right
    graph.Add(ab);
    graph.Add(bc);
    graph.Add(cd);
    graph.Add(asa);
    graph.Add(csa);
    graph.Add(dsa);
    graph.Remove(bc);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &asa}, {&cd, &csa, &dsa}));
    CheckResult(graph.Validate());
    //Make sure reference count for static node works
    graph.Remove(csa);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab, &asa}, {&cd, &dsa}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Removal with edges on both sides that DOESN'T lead to a split
    graph.Add(ab);
    graph.Add(ac);
    graph.Add(cd);
    graph.Add(bd);
    graph.Add(bc);
    graph.Add(asa);
    graph.Add(bsa);
    graph.Remove(bc);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(Match(contentsA, {&ab, &ac, &cd, &bd, &asa, &bsa}));
    CheckResult(graph.Validate());

    return TEST_FAILED;
  }

  bool TestIslandRemoveObject() {
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
    staticA.SetRigidbodyEnabled(false);
    staticB.SetRigidbodyEnabled(false);

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
    graph.Add(ab);
    graph.Remove(a);
    CheckResult(graph.IslandCount() == 0);
    CheckResult(graph.Validate());

    //Removal in middle of chain resulting in size 2 islands on both sides
    graph.Add(ab);
    graph.Add(bc);
    graph.Add(cd);
    graph.Add(de);
    graph.Remove(c);
    CheckResult(graph.IslandCount() == 2);
    graph.GetIsland(0, contentsA);
    graph.GetIsland(1, contentsB);
    CheckResult(Match(contentsA, contentsB, {&ab}, {&de}));
    CheckResult(graph.Validate());

    graph.Clear();

    //Remove center of wheel with a size two island and a bunch of lone nodes
    graph.Add(ab);
    graph.Add(ac);
    graph.Add(bc);
    graph.Add(ad);
    graph.Add(ae);
    graph.Add(af);
    graph.Remove(a);
    CheckResult(graph.IslandCount() == 1);
    graph.GetIsland(0, contentsA);
    CheckResult(Match(contentsA, {&bc}));
    CheckResult(graph.Validate());

    return TEST_FAILED;
  }

  bool TestIslandSleep() {
    TEST_FAILED = false;
    Handle id = 0;
    PhysicsObject a(id++);
    PhysicsObject b(id++);
    PhysicsObject c(id++);
    PhysicsObject d(id++);

    PhysicsObject staticA(id++);
    staticA.SetRigidbodyEnabled(false);

    IslandContents contents;
    IslandGraph graph;

    Constraint ab(ConstraintType::Invalid, &a, &b, id++);
    Constraint bc(ConstraintType::Invalid, &b, &c, id++);
    Constraint cd(ConstraintType::Invalid, &c, &d, id++);
    Constraint saa(ConstraintType::Invalid, &staticA, &a, id++);
    Constraint sad(ConstraintType::Invalid, &staticA, &d, id++);

    //Test basic sleep transition on single island without composure changes.
    graph.Add(ab);
    graph.GetIsland(0, contents);
    CheckResult(contents.mSleepState == SleepState::Awake);

    //Inactive not enough to sleep
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*0.5f);
    graph.GetIsland(0, contents);
    //Not enough to sleep yet, but should have switched from awake to active now that Awake is not a new state anymore
    CheckResult(contents.mSleepState == SleepState::Active);

    //Inactive enough to sleep
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep);
    graph.GetIsland(0, contents);
    CheckResult(contents.mSleepState == SleepState::Asleep);

    //Inactive when sleeping
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep);
    graph.GetIsland(0, contents);
    CheckResult(contents.mSleepState == SleepState::Inactive);

    //Wake from sleep
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Active, 0.0f);
    graph.GetIsland(0, contents);
    CheckResult(contents.mSleepState == SleepState::Awake);

    //Make sure additions lead to wake up
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);
    graph.Add(bc);
    graph.Add(cd);
    graph.Add(saa);
    graph.Add(sad);
    graph.GetIsland(0, contents);
    CheckResult(contents.mSleepState == SleepState::Awake);

    //Make sure removals lead to wake up
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);
    graph.Remove(bc);
    graph.GetIsland(0, contents);
    CheckResult(contents.mSleepState == SleepState::Awake);
    graph.GetIsland(1, contents);
    CheckResult(contents.mSleepState == SleepState::Awake);

    //Test force wake non-static node
    graph.Add(bc);
    graph.GetIsland(0, contents);
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);;
    graph.WakeIsland(b);
    graph.GetIsland(0, contents);
    CheckResult(contents.mSleepState == SleepState::Awake);

    //Test force wake static node
    graph.Remove(bc);
    //Put both islands to sleep
    graph.GetIsland(0, contents);
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);
    graph.GetIsland(1, contents);
    graph.UpdateIslandState(contents.mIslandKey, SleepState::Inactive, Island::sTimeToSleep*2.0f);
    graph.WakeIsland(staticA);
    graph.GetIsland(0, contents);
    CheckResult(contents.mSleepState == SleepState::Awake);
    graph.GetIsland(1, contents);
    CheckResult(contents.mSleepState == SleepState::Awake);

    return TEST_FAILED;
  }

  bool TestIslandAll() {
    bool add = TestIslandAdd();
    bool remCon = TestIslandRemoveConstraint();
    bool remObj = TestIslandRemoveObject();
    bool sleep = TestIslandSleep();
    return add || remCon || remObj || sleep;
  }
}