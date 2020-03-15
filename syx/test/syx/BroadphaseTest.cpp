#include "Precompile.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
//TODO: solve includes for syx so this careful order isn't necessary
#include "../syx/Precompile.h"
#include "SyxAABBTree.h"

namespace Syx {
  bool operator==(const ResultNode& lhs, const ResultNode& rhs) {
    return lhs.mHandle == rhs.mHandle && lhs.mUserdata == rhs.mUserdata;
  }
}

namespace SyxTest {
  TEST_CLASS(BroadphaseTest) {
  public:
    //Equality that ignores order in container and order within pair
    bool _equals(const std::vector<std::pair<Syx::ResultNode, Syx::ResultNode>>& l, const std::vector<std::pair<Syx::Handle, Syx::Handle>>& r) const {
      if(l.size() == r.size()) {
        return std::all_of(l.begin(), l.end(), [&r](const std::pair<Syx::ResultNode, Syx::ResultNode>& pair) {
          return std::find_if(r.begin(), r.end(), [&pair](const std::pair<Syx::Handle, Syx::Handle>& other) {
            return (pair.first.mHandle == other.first && pair.second.mHandle == other.second) || (pair.first.mHandle == other.second && pair.second.mHandle == other.first);
          }) != r.end();
        });
      }
      return false;
    }

    void _assertQueryResults(const Syx::Broadphase& broadphase, const Syx::BoundingVolume& queryVolume, const std::vector<Syx::ResultNode>& expectedHits, const std::vector<std::pair<Syx::ResultNode, Syx::ResultNode>>& expectedPairs) {
      auto pair = broadphase.createPairContext();
      auto raycast = broadphase.createHitContext();
      auto volume = broadphase.createHitContext();

      pair->queryPairs();
      //Cast from outside the volume into it
      raycast->queryRaycast(queryVolume.mAABB.getCenter() + queryVolume.mAABB.getDiagonal(), queryVolume.mAABB.getCenter());
      volume->queryVolume(queryVolume);

      Assert::IsTrue(expectedPairs == pair->get(), L"Pair query should match", LINE_INFO());
      Assert::IsTrue(expectedHits == raycast->get(), L"Raycast query should match", LINE_INFO());
      Assert::IsTrue(expectedHits == volume->get(), L"Volume query should match", LINE_INFO());
    }

    TEST_METHOD(Broadphase_AddOne_ShowsInQueries) {
      auto broadphase = Syx::Create::aabbTree();
      Syx::BoundingVolume volume(Syx::AABB(Syx::Vec3(0.0f), Syx::Vec3(1.0f)));
      Syx::Handle handle = broadphase->insert(volume, nullptr);
      std::vector<Syx::ResultNode> expectedHits;
      expectedHits.push_back({ handle, nullptr });
      _assertQueryResults(*broadphase, volume, expectedHits, {});
    }

    TEST_METHOD(Broadphase_AddRemoveOne_IsEmpty) {
      auto broadphase = Syx::Create::aabbTree();
      Syx::BoundingVolume volume(Syx::AABB(Syx::Vec3(0.0f), Syx::Vec3(1.0f)));
      Syx::Handle handle = broadphase->insert(volume, nullptr);
      broadphase->remove(handle);
      //Results should be empty
      _assertQueryResults(*broadphase, volume, {}, {});
    }

    TEST_METHOD(Brodphase_AddOneClear_IsEmpty) {
      auto broadphase = Syx::Create::aabbTree();
      const Syx::BoundingVolume volume(Syx::AABB(Syx::Vec3(0.0f), Syx::Vec3(1.0f)));
      broadphase->insert(volume, nullptr);
      broadphase->clear();
      //Results should be empty
      _assertQueryResults(*broadphase, volume, {}, {});
    }

    TEST_METHOD(Broadphase_AddPair_ShowsInQuery) {
      auto broadphase = Syx::Create::aabbTree();
      const Syx::BoundingVolume volume(Syx::AABB(Syx::Vec3(0.0f), Syx::Vec3(1.0f)));
      const Syx::Handle a = broadphase->insert(volume, nullptr);
      const Syx::Handle b = broadphase->insert(volume, nullptr);

      auto context = broadphase->createPairContext();
      context->queryPairs();
      Assert::IsTrue(_equals(context->get(), { { a, b } }), L"Added pair should show up in query");
    }

    TEST_METHOD(Broadphase_DistantPairs_LocalPairShowsInQuery) {
      auto broadphase = Syx::Create::aabbTree();
      const Syx::BoundingVolume volumeA(Syx::AABB(Syx::Vec3(0.0f), Syx::Vec3(1.0f)));
      auto pairA = std::make_pair(broadphase->insert(volumeA, nullptr), broadphase->insert(volumeA, nullptr));
      const Syx::BoundingVolume volumeB(Syx::AABB(Syx::Vec3(1000.0f), Syx::Vec3(1001.0f)));
      auto pairB = std::make_pair(broadphase->insert(volumeB, nullptr), broadphase->insert(volumeB, nullptr));
      auto context = broadphase->createPairContext();

      context->queryPairs();
      Assert::IsTrue(context->get().size() == 2, L"There should only be two pairs since they are both far away from each other");
      Assert::IsTrue(_equals(context->get(), { pairA, pairB }), L"Pairs should match based volumes");
    }

    TEST_METHOD(Broadphase_MoveFromOneAreaToOther_ResultsInNewArea) {
      auto broadphase = Syx::Create::aabbTree();
      const Syx::BoundingVolume volumeA(Syx::AABB(Syx::Vec3(0.0f), Syx::Vec3(1.0f)));
      auto pairA = std::make_pair(broadphase->insert(volumeA, nullptr), broadphase->insert(volumeA, nullptr));
      const Syx::BoundingVolume volumeB(Syx::AABB(Syx::Vec3(1000.0f), Syx::Vec3(1001.0f)));
      auto pairB = std::make_pair(broadphase->insert(volumeB, nullptr), broadphase->insert(volumeB, nullptr));
      auto context = broadphase->createPairContext();

      const Syx::Handle toMove = pairB.first;
      const Syx::Handle moved = broadphase->update(volumeA, toMove);

      context->queryPairs();
      Assert::IsTrue(_equals(context->get(), { pairA, { pairA.first, moved }, { pairA.second, moved } }), L"Moved item should show up in query at destination");
    }
  };
}