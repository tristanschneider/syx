#include "Precompile.h"
#include "CppUnitTest.h"

#include "SyxResourceHandle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Syx;

namespace SyxTests {
  TEST_CLASS(ResourceHandleTests) {
    struct TestResource : public EnableDeferredDeletion<TestResource> {
      using Handle = DeferredDeleteResourceHandle<TestResource>;
      using EnableDeferredDeletion::EnableDeferredDeletion;
    };

    struct TestResourceHandle : public DeferredDeleteResourceHandle<TestResource> {
      using DeferredDeleteResourceHandle::DeferredDeleteResourceHandle;

      TestResource& get() {
        return _get();
      }
    };

    TEST_METHOD(DeferredDeleteHandle_CreateHandle_IsNotMarkedForDeletion) {
      TestResourceHandle handle;
      TestResource resource(handle);

      Assert::IsFalse(resource.isMarkedForDeletion(), L"Resource should not be marked for deletion if handle still exists", LINE_INFO());
      Assert::IsTrue(&resource == &handle.get(), L"Handle should be pointing at resource", LINE_INFO());
    }

    TEST_METHOD(DeferredDeleteHandle_DestroyHandle_IsMarkedForDeletion) {
      auto handle = std::make_unique<TestResource::Handle>();
      TestResource resource(*handle);

      handle.reset();

      Assert::IsTrue(resource.isMarkedForDeletion(), L"Resource should be marked for deletion if handle is deleted");
    }

    TEST_METHOD(DeferredDeleteHandle_DestroyResource_HandlerDoesntCrash) {
      auto handle = std::make_unique<TestResource::Handle>();
      auto resource = std::make_unique<TestResource>(*handle);

      resource.reset();
      handle.reset();
    }

    TEST_METHOD(DeferredDeleteHandle_MoveConstruct_IsMarkedForDeletionOnce) {
      auto handle = std::make_unique<TestResource::Handle>();
      TestResource resource(*handle);

      auto copy = std::make_unique<TestResource::Handle>(*handle);
      handle.reset();

      Assert::IsFalse(resource.isMarkedForDeletion(), L"Copy should be keeping resource alive", LINE_INFO());

      copy.reset();
      Assert::IsTrue(resource.isMarkedForDeletion(), L"Resource should be marked for deletion when all handles expire", LINE_INFO());
    }

    TEST_METHOD(DeferredDeleteHandle_MoveAssign_OriginalAndSourceMarkedForDeletion) {
      auto handle = std::make_unique<TestResource::Handle>();
      TestResource resourceA(*handle);
      auto handleB = std::make_unique<TestResource::Handle>();
      TestResource resourceB(*handleB);

      *handleB = std::move(*handle);

      Assert::IsTrue(resourceB.isMarkedForDeletion(), L"B should be marked for deletion since the relevant handle is now pointing at B", LINE_INFO());
      Assert::IsFalse(resourceA.isMarkedForDeletion(), L"A should not be marked for deletion because B is still pointing at it", LINE_INFO());

      handleB.reset();
      Assert::IsTrue(resourceA.isMarkedForDeletion(), L"A should be marked for deletion once no handles reference it anymore", LINE_INFO());
    }

    TEST_METHOD(PerformDeferredDeletions_Compiles) {
      std::vector<std::shared_ptr<TestResource>> a;
      std::vector<TestResource> b;
      std::vector<TestResource*> c;
      std::unordered_map<int, TestResource> d;
      std::unordered_map<int, std::unique_ptr<TestResource>> e;

      TestResource::performDeferredDeletions(a);
      TestResource::performDeferredDeletions(b);
      TestResource::performDeferredDeletions(c);
      TestResource::performDeferredDeletions(d);
      TestResource::performDeferredDeletions(e);
    }
  };
}