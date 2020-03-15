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
    void _assertQueryResults(const Syx::Broadphase& broadphase, const Syx::BoundingVolume& queryVolume, const std::vector<Syx::ResultNode>& expectedHits, const std::vector<std::pair<Syx::ResultNode, Syx::ResultNode>>& expectedPairs) {
      auto pair = broadphase.createPairContext();
      auto raycast = broadphase.createHitContext();
      auto volume = broadphase.createHitContext();

      pair->queryPairs();
      // Cast from outside the volume into it
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
      // Results should be empty
      _assertQueryResults(*broadphase, volume, {}, {});
    }
  };
}