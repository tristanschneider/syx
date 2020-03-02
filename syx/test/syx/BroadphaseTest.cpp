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
    // TODO: context is dangerous
    void _assertQueryResults(const Syx::Broadphase& broadphase, const Syx::BoundingVolume& queryVolume, const Syx::BroadphaseContext& expected) {
      Syx::AABBTreeContext pair, raycast, volume;
      broadphase.queryPairs(pair);
      // Cast from outside the volume into it
      broadphase.queryRaycast(queryVolume.mAABB.getCenter() + queryVolume.mAABB.getDiagonal(), queryVolume.mAABB.getCenter(), raycast);
      broadphase.queryVolume(queryVolume, volume);

      Assert::IsTrue(expected.mQueryPairResults == pair.mQueryPairResults, L"Pair query should match", LINE_INFO());
      Assert::IsTrue(expected.mQueryResults == raycast.mQueryResults, L"Raycast query should match", LINE_INFO());
      Assert::IsTrue(expected.mQueryResults == volume.mQueryResults, L"Volume query should match", LINE_INFO());
    }

    TEST_METHOD(Broadphase_AddOne_ShowsInQueries) {
      Syx::AABBTree broadphase;
      Syx::BoundingVolume volume(Syx::AABB(Syx::Vec3(0.0f), Syx::Vec3(1.0f)));
      Syx::Handle handle = broadphase.insert(volume, nullptr);
      Syx::AABBTreeContext expected;
      expected.mQueryResults.push_back({ handle, nullptr });
      _assertQueryResults(broadphase, volume, expected);
    }

    TEST_METHOD(Broadphase_AddRemoveOne_IsEmpty) {
      Syx::AABBTree broadphase;
      Syx::BoundingVolume volume(Syx::AABB(Syx::Vec3(0.0f), Syx::Vec3(1.0f)));
      Syx::Handle handle = broadphase.insert(volume, nullptr);
      broadphase.remove(handle);
      // Results should be empty
      _assertQueryResults(broadphase, volume, Syx::AABBTreeContext());
    }
  };
}