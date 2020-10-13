#include "Precompile.h"
#include "CppUnitTest.h"

#include "registry/IDRegistry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UtilTests {
  TEST_CLASS(IDRegistryTests) {
  public:
    TEST_METHOD(IDRegistry_GenerateID_IsNotNull) {
      auto registry = create::idRegistry();
      Assert::IsTrue(registry->generateNewUniqueID() != nullptr, L"Generate should always succeed", LINE_INFO());
    }

    TEST_METHOD(IDRegistry_ClaimID_IsNotNull) {
      auto registry = create::idRegistry();
      Assert::IsTrue(registry->tryClaimKnownID(UniqueID(55)) != nullptr, L"tryClaim should succeed if the id is unclaimed", LINE_INFO());
    }

    TEST_METHOD(IDRegistry_ClaimGeneratedID_Fails) {
      auto registry = create::idRegistry();
      auto generated = registry->generateNewUniqueID();
      Assert::IsTrue(registry->tryClaimKnownID(**generated) == nullptr, L"Claiming a generated id should fail because the id is already taken", LINE_INFO());
    }

    TEST_METHOD(IDRegistry_ClaimClaimedID_Fails) {
      auto registry = create::idRegistry();
      auto claimed = registry->tryClaimKnownID(55);
      Assert::IsTrue(registry->tryClaimKnownID(**claimed) == nullptr, L"Claiming an id that has already been claimed should fail because the id is taken", LINE_INFO());
    }

    TEST_METHOD(IDRegistry_ClaimExpiredID_Succeeds) {
      auto registry = create::idRegistry();
      registry->tryClaimKnownID(55);
      Assert::IsTrue(registry->tryClaimKnownID(55) != nullptr, L"Claiming an expired id should work because expiration should have made the id available again", LINE_INFO());
    }

    TEST_METHOD(IDRegistry_IDExpiresAfterRegistry_DoesntCrash) {
      auto registry = create::idRegistry();
      auto id = registry->generateNewUniqueID();
      registry = nullptr;
      id = nullptr;
    }

    TEST_METHOD(IDRegistry_GenerateMultithreaded_AllUnique) {
      struct ThreadData {
        std::unique_ptr<std::thread> mThread;
        std::vector<std::unique_ptr<IClaimedUniqueID>> mIDs;
      };
      constexpr size_t numThreads = 4;
      constexpr size_t numIds = 1000;
      auto registry = create::idRegistry();

      std::array<ThreadData, numThreads> threads;
      for(ThreadData& thread : threads) {
        thread.mThread = std::make_unique<std::thread>([numIds, &thread, &registry] {
          for(size_t i = 0; i < numIds; ++i) {
             thread.mIDs.emplace_back(registry->generateNewUniqueID());
          }
        });
      }

      std::unordered_set<UniqueID> uniqueIDs;
      uniqueIDs.reserve(numIds * numThreads);
      for(ThreadData& thread : threads) {
        thread.mThread->join();
        for(auto& id : thread.mIDs) {
          Assert::IsTrue(uniqueIDs.insert(**id).second, L"All generated ids should be unique", LINE_INFO());
        }
        thread.mThread = nullptr;
        thread.mIDs.clear();
      }
    }
  };
}