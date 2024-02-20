#include "Precompile.h"
#include "CppUnitTest.h"

#include "SweepNPrune.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(BroadphaseTest) {
    struct SweepEntry {
      glm::vec2 mNewBoundaryMin{};
      glm::vec2 mNewBoundaryMax{};
      size_t mKey{};
      Broadphase::BroadphaseKey broadphaseKey{ Broadphase::ObjectDB::EMPTY_KEY };
    };

    struct TestSweep {
      void update(Broadphase::SwapLog& log) {
        Broadphase::SweepNPrune::recomputePairs(sweep, objects, pairs, log);
        Broadphase::logPendingRemovals(objects, log, pairs);
        std::vector<Broadphase::SweepCollisionPair> tempGain, tempLoss;
        tempGain.swap(log.gains);
        tempLoss.swap(log.losses);
        Broadphase::logChangedPairs(objects, pairs, { tempGain, tempLoss }, log);
        Broadphase::processPendingRemovals(objects);
      }

      Broadphase::Sweep2D sweep;
      Broadphase::ObjectDB objects;
      Broadphase::PairTracker pairs;
    };

    static void _assertSorted(const TestSweep& sweep) {
      std::array<std::vector<float>, Broadphase::Sweep2D::S> values;
      for(size_t d = 0; d < Broadphase::Sweep2D::S; ++d) {
        for(size_t i = 0; i < sweep.sweep.axis[d].elements.size(); ++i) {
          const Broadphase::SweepElement& k = sweep.sweep.axis[d].elements[i];
          const auto& pair = sweep.objects.bounds[d][k.getValue()];
          values[d].push_back(k.isStart() ? pair.first : pair.second);
        }
      }

      for(size_t d = 0; d < Broadphase::Sweep2D::S; ++d) {
        Assert::IsTrue(std::is_sorted(values[d].begin(), values[d].end()), L"All elements should be sorted by bounds");
        std::vector<Broadphase::SweepElement> elements = sweep.sweep.axis[d].elements;
        std::sort(elements.begin(), elements.end());
        Assert::IsTrue(std::unique(elements.begin(), elements.end()) == elements.end(), L"There should be no duplicate elements");
      }
    }

    static void _clear(std::vector<Broadphase::SweepCollisionPair>& a, std::vector<Broadphase::SweepCollisionPair>& b) {
      a.clear();
      b.clear();
    }

    static void _insertOne(TestSweep& sweep, SweepEntry& entry, std::vector<Broadphase::SweepCollisionPair>& gained) {
      if(entry.broadphaseKey == Broadphase::ObjectDB::EMPTY_KEY) {
        Broadphase::insertRange(sweep.objects, &entry.mKey, &entry.broadphaseKey, 1);
        Broadphase::SweepNPrune::insertRange(sweep.sweep, &entry.broadphaseKey, 1);
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

    static void _reinsertOne(TestSweep& sweep, SweepEntry& entry, std::vector<Broadphase::SweepCollisionPair>& gained, std::vector<Broadphase::SweepCollisionPair>& lost) {
      Broadphase::updateBoundaries(sweep.objects, &entry.mNewBoundaryMin.x, &entry.mNewBoundaryMax.x, &entry.mNewBoundaryMin.y, &entry.mNewBoundaryMax.y, &entry.broadphaseKey, 1);
      Broadphase::SwapLog log{ gained, lost };
      sweep.update(log);
      _assertSorted(sweep);
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
      entry.mKey = size_t(1);
      entry.mNewBoundaryMin = glm::vec2(1.0f, 2.0f);
      entry.mNewBoundaryMax = glm::vec2(2.0f, 3.0f);
      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());

      //Move out of contact on one axis
      entry.mNewBoundaryMax.x += 5.0f;
      entry.mNewBoundaryMin.x += 5.0f;
      entry.mKey = size_t(2);
      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());

      _eraseOne(sweep, entry, pairs);

      Assert::IsTrue(pairs.empty());
    }

    static Broadphase::SweepCollisionPair sweepPair(size_t a, size_t b) {
      return Broadphase::SweepCollisionPair{ a, b };
    }

    TEST_METHOD(SweepNPrune_InsertRange) {
      TestSweep sweep;
      std::vector<Broadphase::SweepCollisionPair> pairs;
      SweepEntry entry;
      entry.mKey = size_t(1);
      entry.mNewBoundaryMin = glm::vec2(1.0f, 2.0f);
      entry.mNewBoundaryMax = glm::vec2(2.0f, 3.0f);
      SweepEntry same = entry;
      same.mKey = size_t(2);

      _insertOne(sweep, entry, pairs);
      Assert::IsTrue(pairs.empty());
      //Insert another at the same coordinates, should cause new pair
      _insertOne(sweep, same, pairs);
      assertPairsMatch(pairs, { sweepPair(1, 2) });
      pairs.clear();

      //Insert one to the left of both of the previous ones
      SweepEntry left;
      left.mNewBoundaryMin = entry.mNewBoundaryMin - glm::vec2(2.0f);
      left.mNewBoundaryMax = left.mNewBoundaryMin + glm::vec2(1.0f);
      left.mKey = 3;
      _insertOne(sweep, left, pairs);
      assertPairsMatch(pairs, {});

      //Insert one to the right of all of the previous
      SweepEntry right;
      right.mNewBoundaryMin = entry.mNewBoundaryMax + glm::vec2(1.0f);
      right.mNewBoundaryMax = right.mNewBoundaryMin + glm::vec2(1.0f);
      right.mKey = 4;
      _insertOne(sweep, right, pairs);
      assertPairsMatch(pairs, {});

      //Insert on the boundary of left and center
      SweepEntry leftToCenter;
      leftToCenter.mNewBoundaryMin = left.mNewBoundaryMax;
      leftToCenter.mNewBoundaryMax = entry.mNewBoundaryMin;
      leftToCenter.mKey = 5;
      _insertOne(sweep, leftToCenter, pairs);
      assertPairsMatch(pairs, { sweepPair(5, 1), sweepPair(5, 2), sweepPair(5, 3) });
      pairs.clear();

      //Entirely containing right
      SweepEntry rightOverlap;
      rightOverlap.mNewBoundaryMin = right.mNewBoundaryMin - glm::vec2(0.1f);
      rightOverlap.mNewBoundaryMax = right.mNewBoundaryMax + glm::vec2(0.1f);
      rightOverlap.mKey = 6;
      _insertOne(sweep, rightOverlap, pairs);
      assertPairsMatch(pairs, { sweepPair(6, 4) });
      pairs.clear();

      //Contained by right and rightOVerlap
      SweepEntry rightContained;
      rightContained.mNewBoundaryMin = right.mNewBoundaryMin + glm::vec2(0.1f);
      rightContained.mNewBoundaryMax = rightContained.mNewBoundaryMin + glm::vec2(0.1f);
      rightContained.mKey = 7;
      _insertOne(sweep, rightContained, pairs);
      assertPairsMatch(pairs, { sweepPair(7, 4), sweepPair(7, 6) });
      pairs.clear();

      std::vector<Broadphase::SweepCollisionPair> lostPairs;
      _eraseOne(sweep, rightContained, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(7, 4), sweepPair(7, 6) });
      lostPairs.clear();

      _eraseOne(sweep, rightOverlap, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(6, 4) });
      lostPairs.clear();

      _eraseOne(sweep, leftToCenter, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(5, 1), sweepPair(5, 2), sweepPair(5, 3) });
      lostPairs.clear();

      _eraseOne(sweep, right, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();

      _eraseOne(sweep, left, lostPairs);
      assertPairsMatch(lostPairs, {});
      lostPairs.clear();

      _eraseOne(sweep, same, lostPairs);
      assertPairsMatch(lostPairs, { sweepPair(1, 2) });
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
      upperLeft.mKey = 1;
      upperLeft.mNewBoundaryMin = glm::vec2(0.0f, size + space);
      upperLeft.mNewBoundaryMax = upperLeft.mNewBoundaryMin + glm::vec2(size);

      SweepEntry upperRight;
      upperRight.mKey = 2;
      upperRight.mNewBoundaryMin = glm::vec2(size + space, size + space);
      upperRight.mNewBoundaryMax = upperRight.mNewBoundaryMin + glm::vec2(size);

      SweepEntry bottomLeft;
      bottomLeft.mKey = 3;
      bottomLeft.mNewBoundaryMin = glm::vec2(0.0f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMin + glm::vec2(size);

      SweepEntry bottomRight;
      bottomRight.mKey = 4;
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
      assertPairsMatch(gainedPairs, { sweepPair(2, 1), sweepPair(2, 3), sweepPair(2, 4) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Move bottom left away from all
      bottomLeft.mNewBoundaryMax -= glm::vec2(100.0f);
      bottomLeft.mNewBoundaryMin -= glm::vec2(100.0f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(3, 2) });
      _clear(gainedPairs, lostPairs);

      //Undo the previous move
      bottomLeft.mNewBoundaryMax += glm::vec2(100.0f);
      bottomLeft.mNewBoundaryMin += glm::vec2(100.0f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(3, 2) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Move right to within lower right, but out of the left two
      upperRight.mNewBoundaryMin.x = bottomRight.mNewBoundaryMin.x + 0.1f;
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(2, 1), sweepPair(2, 3) });
      _clear(gainedPairs, lostPairs);

      //Restore right to how it started
      upperRight.mNewBoundaryMin = glm::vec2(size + space, size + space);
      upperRight.mNewBoundaryMax = upperRight.mNewBoundaryMin + glm::vec2(size);
      _reinsertOne(sweep, upperRight, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(2, 4) });
      _clear(gainedPairs, lostPairs);

      //Extend bottom left up into upper right, overlapping with everything
      bottomLeft.mNewBoundaryMax += glm::vec2(size * 0.5f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(3, 1), sweepPair(3, 2), sweepPair(3, 4) });
      assertPairsMatch(lostPairs, {});
      _clear(gainedPairs, lostPairs);

      //Shrink it back on top so it's only overlapping with bottom right
      bottomLeft.mNewBoundaryMax.y = bottomRight.mNewBoundaryMax.y;
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, {});
      assertPairsMatch(lostPairs, { sweepPair(3, 1), sweepPair(3, 2) });
      _clear(gainedPairs, lostPairs);

      //Resize and move bottom left to inside of upperRight
      bottomLeft.mNewBoundaryMin = upperRight.mNewBoundaryMin + glm::vec2(0.1f);
      bottomLeft.mNewBoundaryMax = bottomLeft.mNewBoundaryMax + glm::vec2(0.1f);
      _reinsertOne(sweep, bottomLeft, gainedPairs, lostPairs);
      assertPairsMatch(gainedPairs, { sweepPair(3, 2) });
      assertPairsMatch(lostPairs, { sweepPair(3, 4) });
    }
  };
}