#include "Precompile.h"
#include "CppUnitTest.h"

#include "SweepNPrune.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(BroadphaseTest) {
    struct SweepEntry {
      glm::vec2 mNewBoundaryMin{};
      glm::vec2 mNewBoundaryMax{};
      Broadphase::UserKey mKey{};
      Broadphase::BroadphaseKey broadphaseKey{ Broadphase::ObjectDB::EMPTY_KEY };
    };

    struct TestSweep {
      void update(Broadphase::SwapLog& log) {
        std::vector<Broadphase::SweepKeyPair> tempGain, tempLoss;
        Broadphase::IntermediateLog temp{ tempGain, tempLoss };
        Broadphase::ConstIntermediateLog ctemp{ tempGain, tempLoss };
        Broadphase::SweepNPrune::recomputePairs(sweep, objects, pairs, temp);
        Broadphase::logPendingRemovals(objects, temp, pairs);
        Broadphase::logChangedPairs(objects, pairs, ctemp, log);
        Broadphase::processPendingRemovals(objects);
      }

      float unwrap(size_t axis, const Broadphase::SweepElement& e) const {
        const auto& pair = objects.bounds[axis][e.getValue()];
        return e.isStart() ? pair.first : pair.second;
      }

      Broadphase::UserKey createKey() {
        return mappings.createKey();
      }

      Broadphase::Sweep2D sweep;
      Broadphase::ObjectDB objects;
      Broadphase::PairTracker pairs;
      StableElementMappings mappings;
    };

    struct TestElementSort {
      bool operator()(const Broadphase::SweepElement& l, const Broadphase::SweepElement& r) const {
        if(l.getValue() == r.getValue()) {
          return l.isStart();
        }
        return sweep.unwrap(axis, l) < sweep.unwrap(axis, r);
      }

      const TestSweep& sweep;
      size_t axis{};
    };

    static void _assertSorted(const TestSweep& sweep) {
      std::array<std::vector<float>, Broadphase::Sweep2D::S> values;
      for(size_t d = 0; d < Broadphase::Sweep2D::S; ++d) {
        for(size_t i = 0; i < sweep.sweep.axis[d].elements.size(); ++i) {
          values[d].push_back(sweep.unwrap(d, sweep.sweep.axis[d].elements[i]));
        }
      }
      for(size_t d = 0; d < Broadphase::Sweep2D::S; ++d) {
        std::vector<Broadphase::SweepElement> elements = sweep.sweep.axis[d].elements;
        Assert::IsTrue(std::is_sorted(values[d].begin(), values[d].end()));
        Assert::IsTrue(std::is_sorted(elements.begin(), elements.end(), TestElementSort{ sweep, d }), L"All elements should be sorted by bounds");
        std::sort(elements.begin(), elements.end());
        Assert::IsTrue(std::unique(elements.begin(), elements.end()) == elements.end(), L"There should be no duplicate elements");

        Assert::IsTrue(Broadphase::Debug::isValidSweepAxis(sweep.sweep.axis[d]));
      }
    }

    static void _clear(std::vector<Broadphase::SweepCollisionPair>& a, std::vector<Broadphase::SweepCollisionPair>& b) {
      a.clear();
      b.clear();
    }

    static void _insertOne(TestSweep& sweep, SweepEntry& entry, std::vector<Broadphase::SweepCollisionPair>& gained) {
      if(entry.broadphaseKey == Broadphase::ObjectDB::EMPTY_KEY) {
        Broadphase::insertRange(sweep.objects, &entry.mKey, &entry.broadphaseKey, 1);
        Broadphase::SweepNPrune::tryInsertRange(sweep.sweep, &entry.broadphaseKey, 1);
      }
      Broadphase::updateBoundaries(sweep.objects, &entry.mNewBoundaryMin.x, &entry.mNewBoundaryMax.x, &entry.mNewBoundaryMin.y, &entry.mNewBoundaryMax.y, &entry.broadphaseKey, 1);
      std::vector<Broadphase::SweepCollisionPair> lost;
      Broadphase::SwapLog log{ gained, lost };
      sweep.update(log);
      Assert::IsTrue(lost.empty(), L"Nothing should be lost when inserting elements");
      _assertSorted(sweep);
    }

    static void _eraseOne(TestSweep& sweep, SweepEntry& entry, std::vector<Broadphase::SweepCollisionPair>& lost) {
      Broadphase::eraseRange(sweep.objects, &entry.broadphaseKey, 1);
      std::vector<Broadphase::SweepCollisionPair> gained;
      Broadphase::SwapLog log{ gained, lost };
      sweep.update(log);
      Assert::IsTrue(gained.empty(), L"Nothing should be gained when removing an element");
      _assertSorted(sweep);
    }

    static void updateBoundaries(TestSweep& sweep, SweepEntry& entry) {
      Broadphase::updateBoundaries(sweep.objects, &entry.mNewBoundaryMin.x, &entry.mNewBoundaryMax.x, &entry.mNewBoundaryMin.y, &entry.mNewBoundaryMax.y, &entry.broadphaseKey, 1);
    }

    static void update(TestSweep& sweep, std::vector<Broadphase::SweepCollisionPair>& gained, std::vector<Broadphase::SweepCollisionPair>& lost) {
      Broadphase::SwapLog log{ gained, lost };
      sweep.update(log);
      _assertSorted(sweep);
    }

    static void _reinsertOne(TestSweep& sweep, SweepEntry& entry, std::vector<Broadphase::SweepCollisionPair>& gained, std::vector<Broadphase::SweepCollisionPair>& lost) {
      updateBoundaries(sweep, entry);
      update(sweep, gained, lost);
    }

    static bool pairMatches(Broadphase::SweepCollisionPair l, Broadphase::SweepCollisionPair r) {
      return r == l;
    }

    static void assertPairsMatch(std::vector<Broadphase::SweepCollisionPair> l, std::initializer_list<Broadphase::SweepCollisionPair> r) {
      Assert::AreEqual(l.size(), r.size());
      std::sort(l.begin(), l.end());
      std::vector<Broadphase::SweepCollisionPair> rv{ r };
      std::sort(rv.begin(), rv.end());
      Assert::IsTrue(l == rv);
    }

    TEST_METHOD(SweepNPrune_EraseOneOverlappingAxis_NoLoss) {
      TestSweep sweep;
      std::vector<Broadphase::SweepCollisionPair> pairs;
      SweepEntry entry;
      entry.mKey = sweep.createKey();
      entry.mNewBoundaryMin = glm::vec2(1.0f, 2.0f);
      entry.mNewBoundaryMax = glm::vec2(2.0f, 3.0f);
      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());

      //Move out of contact on one axis
      entry.mNewBoundaryMax.x += 5.0f;
      entry.mNewBoundaryMin.x += 5.0f;
      entry.mKey = sweep.createKey();
      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());

      _eraseOne(sweep, entry, pairs);

      Assert::IsTrue(pairs.empty());
    }

    TEST_METHOD(BoundaryRemoval) {
      TestSweep sweep;
      std::vector<Broadphase::SweepCollisionPair> pairs, pairs2;
      auto clearPairs = [&] {
        pairs.clear();
        pairs2.clear();
      };
      for(auto& axis : sweep.sweep.axis) {
        axis.min = 0;
        axis.max = 1;
      }
      SweepEntry b, c, d;
      constexpr float o = 0.5f;
      //On the edge but with one side still in bounds
      {
        SweepEntry t;
        t.mKey = sweep.createKey();
        t.mNewBoundaryMin.y = 0.5f;
        t.mNewBoundaryMax.y = 1.5f;
        t.mNewBoundaryMin.x = t.mNewBoundaryMax.x = o;
        _insertOne(sweep, t, pairs);
      }
      {
        SweepEntry t;
        t.mKey = sweep.createKey();
        t.mNewBoundaryMin = glm::vec2{ -0.5f };
        t.mNewBoundaryMax = glm::vec2{ 0.5f };
        t.mNewBoundaryMin.x = t.mNewBoundaryMax.x = o;
        _insertOne(sweep, t, pairs);
        clearPairs();
      }
      b.mKey = sweep.createKey();
      b.mNewBoundaryMin = b.mNewBoundaryMax = glm::vec2{ 0.25f };
      b.mNewBoundaryMin.x = b.mNewBoundaryMax.x = o;
      _insertOne(sweep, b, pairs);
      clearPairs();
      c.mKey = sweep.createKey();
      c.mNewBoundaryMin = c.mNewBoundaryMax = glm::vec2{ 0.75f };
      c.mNewBoundaryMin.x = c.mNewBoundaryMax.x = o;
      _insertOne(sweep, c, pairs);
      clearPairs();
      d.mKey = sweep.createKey();
      d.mNewBoundaryMin = d.mNewBoundaryMax = glm::vec2{ 0.15f };
      d.mNewBoundaryMin.x = d.mNewBoundaryMax.x = o;
      _insertOne(sweep, d, pairs);
      clearPairs();

      {
        SweepEntry t;
        t.mKey = sweep.createKey();
        t.mNewBoundaryMin = glm::vec2{ 0.5f };
        t.mNewBoundaryMax = glm::vec2{ 1.5f };
        t.mNewBoundaryMin.x = t.mNewBoundaryMax.x = o;
        _insertOne(sweep, t, pairs);
      }

      clearPairs();

      b.mNewBoundaryMin.y = b.mNewBoundaryMax.y = 1.5f;
      //_reinsertOne(sweep, b, pairs, pairs2);
      //clearPairs();
      c.mNewBoundaryMin.y = c.mNewBoundaryMax.y = 1.5f;
      //_reinsertOne(sweep, c, pairs, pairs2);
      //clearPairs();
      d.mNewBoundaryMin.y = d.mNewBoundaryMax.y = -0.5f;
      //_reinsertOne(sweep, d, pairs, pairs2);
      //clearPairs();

      updateBoundaries(sweep, b);
      updateBoundaries(sweep, c);
      updateBoundaries(sweep, d);

      update(sweep, pairs, pairs2);

      Assert::IsTrue(Broadphase::Debug::isValidSweepAxis(sweep.sweep.axis[0]));
      Assert::IsTrue(Broadphase::Debug::isValidSweepAxis(sweep.sweep.axis[1]));
    }

    static Broadphase::SweepCollisionPair sweepPair(Broadphase::UserKey a, Broadphase::UserKey b) {
      return Broadphase::SweepCollisionPair{ a, b };
    }

    static Broadphase::SweepCollisionPair sweepPair(const SweepEntry& a, const SweepEntry& b) {
      return Broadphase::SweepCollisionPair{ a.mKey, b.mKey };
    }

    TEST_METHOD(ZeroSizeElement) {
      TestSweep sweep;
      std::vector<Broadphase::SweepCollisionPair> gains;

      {
        SweepEntry entry;
        entry.mKey = sweep.createKey();
        entry.mNewBoundaryMin = entry.mNewBoundaryMax = glm::vec2{ 0 };
        //The _assertSorted in here will catch failures
        _insertOne(sweep, entry, gains);
      }
      {
        SweepEntry entry;
        entry.mKey = sweep.createKey();
        entry.mNewBoundaryMin = entry.mNewBoundaryMax = glm::vec2{ -1.0f };
        _insertOne(sweep, entry, gains);
      }
      {
        SweepEntry entry;
        entry.mKey = sweep.createKey();
        entry.mNewBoundaryMin = entry.mNewBoundaryMax = glm::vec2{ -1.0f };
        _insertOne(sweep, entry, gains);
      }
    }

    TEST_METHOD(SweepNPrune_InsertRange) {
      TestSweep sweep;
      std::vector<Broadphase::SweepCollisionPair> pairs;
      SweepEntry entry;
      entry.mKey = sweep.createKey(); // 1
      entry.mNewBoundaryMin = glm::vec2(1.0f, 2.0f);
      entry.mNewBoundaryMax = glm::vec2(2.0f, 3.0f);
      constexpr float e = 0.000001f;
      constexpr glm::vec2 ev{ e };
      SweepEntry same = entry;
      same.mKey = sweep.createKey(); // 2

      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());
      //Insert another at the same coordinates, should cause new pair
      _insertOne(sweep, same, pairs);
      assertPairsMatch(pairs, { sweepPair(entry.mKey, same.mKey) });
      pairs.clear();

      //Insert one to the left of both of the previous ones
      SweepEntry left;
      left.mNewBoundaryMin = entry.mNewBoundaryMin - glm::vec2(2.0f);
      left.mNewBoundaryMax = left.mNewBoundaryMin + glm::vec2(1.0f);
      left.mKey = sweep.createKey(); // 3
      _insertOne(sweep, left, pairs);
      assertPairsMatch(pairs, {});

      //Insert one to the right of all of the previous
      SweepEntry right;
      right.mNewBoundaryMin = entry.mNewBoundaryMax + glm::vec2(1.0f);
      right.mNewBoundaryMax = right.mNewBoundaryMin + glm::vec2(1.0f);
      right.mKey = sweep.createKey(); // 4
      _insertOne(sweep, right, pairs);
      assertPairsMatch(pairs, {});

      //Insert on the boundary of left and center
      SweepEntry leftToCenter;
      leftToCenter.mNewBoundaryMin = left.mNewBoundaryMax - ev;
      leftToCenter.mNewBoundaryMax = entry.mNewBoundaryMin;
      leftToCenter.mKey = sweep.createKey(); // 5
      _insertOne(sweep, leftToCenter, pairs);
      assertPairsMatch(pairs, { sweepPair(leftToCenter.mKey, entry.mKey), sweepPair(leftToCenter.mKey, same.mKey), sweepPair(leftToCenter.mKey, left.mKey) });
      pairs.clear();

      //Entirely containing right
      SweepEntry rightOverlap;
      rightOverlap.mNewBoundaryMin = right.mNewBoundaryMin - glm::vec2(0.1f);
      rightOverlap.mNewBoundaryMax = right.mNewBoundaryMax + glm::vec2(0.1f);
      rightOverlap.mKey = sweep.createKey(); // 6
      _insertOne(sweep, rightOverlap, pairs);
      assertPairsMatch(pairs, { sweepPair(rightOverlap.mKey, right.mKey) });
      pairs.clear();

      //Contained by right and rightOVerlap
      SweepEntry rightContained;
      rightContained.mNewBoundaryMin = right.mNewBoundaryMin + glm::vec2(0.1f);
      rightContained.mNewBoundaryMax = rightContained.mNewBoundaryMin + glm::vec2(0.1f);
      rightContained.mKey = sweep.createKey(); // 7
      _insertOne(sweep, rightContained, pairs);
      assertPairsMatch(pairs, { sweepPair(rightContained.mKey, right.mKey), sweepPair(rightContained.mKey, rightOverlap.mKey) });
      pairs.clear();

      std::vector<Broadphase::SweepCollisionPair> lostPairs;
      _eraseOne(sweep, rightContained, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(rightContained.mKey, right.mKey), sweepPair(rightContained.mKey, rightOverlap.mKey) });
      lostPairs.clear();

      _eraseOne(sweep, rightOverlap, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(rightOverlap.mKey, right.mKey) });
      lostPairs.clear();

      _eraseOne(sweep, leftToCenter, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(leftToCenter.mKey, entry.mKey), sweepPair(leftToCenter.mKey, same.mKey), sweepPair(leftToCenter.mKey, left.mKey) });
      lostPairs.clear();

      _eraseOne(sweep, right, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();

      _eraseOne(sweep, left, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();

      _eraseOne(sweep, same, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(entry.mKey, same.mKey) });
      lostPairs.clear();

      _eraseOne(sweep, entry, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();
    }

    TEST_METHOD(SweepNPrune_ReinsertRange) {
      TestSweep sweep;
      std::vector<Broadphase::SweepCollisionPair> gainedPairs, lostPairs;
      const float size = 1.0f;
      const float space = 0.1f;

      SweepEntry upperLeft;
      upperLeft.mKey = sweep.createKey(); // 1
      upperLeft.mNewBoundaryMin = glm::vec2(0.0f, size + space);
      upperLeft.mNewBoundaryMax = upperLeft.mNewBoundaryMin + glm::vec2(size);

      SweepEntry upperRight;
      upperRight.mKey = sweep.createKey(); // 2
      upperRight.mNewBoundaryMin = glm::vec2(size + space, size + space);
      upperRight.mNewBoundaryMax = upperRight.mNewBoundaryMin + glm::vec2(size);

      SweepEntry bottomLeft;
      bottomLeft.mKey = sweep.createKey(); // 3
      bottomLeft.mNewBoundaryMin = glm::vec2(0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);

      SweepEntry bottomRight;
      bottomRight.mKey = sweep.createKey(); // 4
      bottomRight.mNewBoundaryMin = glm::vec2(size + space, 0.0f);
      bottomRight.mNewBoundaryMax = bottomRight.mNewBoundaryMin + glm::vec2(size);

      _insertOne(sweep, upperLeft, gainedPairs);
      _insertOne(sweep, upperRight, gainedPairs);
      _insertOne(sweep, bottomLeft, gainedPairs);
      _insertOne(sweep, bottomRight, gainedPairs);
      Assert::IsTrue(gainedPairs.empty());

      //Reinsert in place, nothing should happen
      _reinsertOne(sweep, upperLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, {});

      //Put bottom left to the right of top right. This makes it lose and gain an axis at the same time relative to top left
      bottomLeft.mNewBoundaryMin = upperRight.mNewBoundaryMin + glm::vec2(size + space*2.0f, 0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, {});

      //Put bottom left to the left of upper left, so it has now completely switched sides on one axis relative to the two top entries
      bottomLeft.mNewBoundaryMin = upperLeft.mNewBoundaryMin - glm::vec2(size + space*2.0f, 0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, {});

      //Put it back
      bottomLeft.mNewBoundaryMin = glm::vec2(0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, {});

      //Extend upperRight down left to contain all others
      upperRight.mNewBoundaryMin = glm::vec2(-1.0f);
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(upperRight, upperLeft), sweepPair(upperRight, bottomLeft), sweepPair(upperRight, bottomRight) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Move bottom left away from all
      bottomLeft.mNewBoundaryMax -= glm::vec2(100.0f);
      bottomLeft.mNewBoundaryMin -= glm::vec2(100.0f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(bottomLeft, upperRight) });
      _clear(gainedPairs, lostPairs);

      //Undo the previous move
      bottomLeft.mNewBoundaryMax += glm::vec2(100.0f);
      bottomLeft.mNewBoundaryMin += glm::vec2(100.0f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(bottomLeft, upperRight) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Move right to within lower right, but out of the left two
      upperRight.mNewBoundaryMin.x = bottomRight.mNewBoundaryMin.x + 0.1f;
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(upperRight, upperLeft), sweepPair(upperRight, bottomLeft) });
      _clear(gainedPairs, lostPairs);

      //Restore right to how it started
      upperRight.mNewBoundaryMin = glm::vec2(size + space, size + space);
      upperRight.mNewBoundaryMax = upperRight.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(upperRight, bottomRight) });
      _clear(gainedPairs, lostPairs);

      //Extend bottom left up into upper right, overlapping with everything
      bottomLeft.mNewBoundaryMax += glm::vec2(size * 0.5f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(bottomLeft, upperLeft), sweepPair(bottomLeft, upperRight), sweepPair(bottomLeft, bottomRight) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Shrink it back on top so it's only overlapping with bottom right
      bottomLeft.mNewBoundaryMax.y = bottomRight.mNewBoundaryMax.y;
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(bottomLeft, upperLeft), sweepPair(bottomLeft, upperRight) });
      _clear(gainedPairs, lostPairs);

      //Resize and move bottom left to inside of upperRight
      bottomLeft.mNewBoundaryMin = upperRight.mNewBoundaryMin + glm::vec2(0.1f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(0.1f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(bottomLeft, upperRight) });
      assertPairsMatch(lostPairs, { sweepPair(bottomLeft, bottomRight) });
    }
  };
}