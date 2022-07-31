#include "Precompile.h"
#include "CppUnitTest.h"

#include "TypeErasedContainer.h"
#include "Util.h"
#include "util/TypeId.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(MetaTests) {
    TEST_METHOD(TypeErasedContainer_get_ReturnsCorrectType) {
      auto container = TypeErasedContainer::create<int, std::vector>();
      std::vector<int>* c = container.get<std::vector<int>>();
      Assert::IsNotNull(c, L"Correct type should result in non-null container", LINE_INFO());
      c->push_back(1);
      Assert::AreEqual(c->front(), 1, L"Value should have been inserted", LINE_INFO());
    }

    struct Tracker {
      Tracker(int* counter = nullptr)
        : mCounter(counter) {
      }
      ~Tracker() {
        if(mCounter) {
          (*mCounter)++;
        }
      }

      int* mCounter = nullptr;
    };

    TEST_METHOD(TypeErasedContainer_Destroy_DestructorCalled) {
      int destructions = 0;

      TypeErasedContainer::create<Tracker, std::vector>().get<std::vector<Tracker>>()->emplace_back(&destructions);

      Assert::AreEqual(destructions, 1, L"Destructor should have been invoked once", LINE_INFO());
    }

    TEST_METHOD(TypeErasedContainer_MoveConstruct_DestructorCalled) {
      int destructions = 0;
      {
        auto original = TypeErasedContainer::create<Tracker, std::vector>();
        original.get<std::vector<Tracker>>()->emplace_back(&destructions);

        TypeErasedContainer other(std::move(original));
        Assert::IsTrue(other.get<std::vector<Tracker>>()->front().mCounter == &destructions, L"Value should have been moved", LINE_INFO());
      }
      Assert::AreEqual(destructions, 1, L"Destructor should have been invoked once", LINE_INFO());
    }

    TEST_METHOD(TypeErasedContainer_MoveAssign_DestructorCalled) {
      int destructions = 0;
      {
        auto original = TypeErasedContainer::create<Tracker, std::vector>();
        original.get<std::vector<Tracker>>()->emplace_back(&destructions);

        auto second = TypeErasedContainer::create<Tracker, std::vector>();
        second.get<std::vector<Tracker>>()->emplace_back(&destructions);

        second = std::move(original);
        Assert::IsTrue(second.get<std::vector<Tracker>>()->front().mCounter == &destructions, L"Value should have been moved", LINE_INFO());
        Assert::AreEqual(destructions, 1, L"Destructor of second tracker should have been invoked", LINE_INFO());
      }
      Assert::AreEqual(destructions, 2, L"Destructor should have been invoked for all data", LINE_INFO());
    }
  };
}