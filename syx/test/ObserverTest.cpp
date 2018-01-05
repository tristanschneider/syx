#include "Precompile.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include <atomic>
#include <algorithm>
#include "util/Observer.h"

namespace ObserverTests {
  struct ArgsStruct {
    ArgsStruct(int i , float f, bool b)
      : mInt(i)
      , mFloat(f)
      , mBool(b) {
    }

    virtual void onEvent() {
      mInt = 1;
    }

    int mInt;
    float mFloat;
    bool mBool;
  };

  struct ArgsStructDerived : public ArgsStruct {
    using ArgsStruct::ArgsStruct;

    virtual void onEvent() override {
      mInt = 2;
    }
  };

  TEST_CLASS(ObserverTest) {
  public:
    TEST_METHOD(Observer_ArgsForward) {
      int i = 10; float f = 5.5f; bool b = true;
      Observer<ArgsStruct> o(i, f, b);
      Assert::AreEqual(o.get().mInt, i);
      Assert::AreEqual(o.get().mFloat, f);
      Assert::AreEqual(o.get().mBool, b);
    }

    TEST_METHOD(Observer_SubjectOutlive) {
      Observer<int>::SubjectType s;
      {
        Observer<int> o(1);
        o.observe(&s);
        Assert::IsTrue(s.get().front() == &o);
        Assert::IsTrue(o.hasSubject());
      }
      Assert::IsTrue(s.get().empty(), L"Observer should have been removed upon destruction", LINE_INFO());
    }

    TEST_METHOD(Observer_ObserverOutlive) {
      Observer<int> o(1);
      {
        Observer<int>::SubjectType s;
        o.observe(&s);
        Assert::IsTrue(s.get().front() == &o);
        Assert::IsTrue(o.hasSubject());
      }
      Assert::IsFalse(o.hasSubject(), L"Subject should have removed itself from observer upon destruction", LINE_INFO());
    }

    TEST_METHOD(Observer_DoubleAdd) {
      Observer<int>::SubjectType s;
      Observer<int> o;
      o.observe(&s);
      o.observe(&s);
      Assert::AreEqual(s.get().size(), (size_t)1, L"Duplicate observation should have been prevented", LINE_INFO());
      Assert::IsTrue(o.hasSubject());
    }

    TEST_METHOD(Observer_Copies) {
      Observer<int>::SubjectType s1;
      Observer<int> o1;
      o1.observe(&s1);

      Observer<int> o2(o1);
      Observer<int>::SubjectType s2;
      Observer<int> o3;
      o3.observe(&s2);

      Assert::AreEqual(s1.get().size(), (size_t)2, L"s1 should have o1, and o2 as observers", LINE_INFO());
      o2 = o3;
      Assert::AreEqual(s1.get().size(), (size_t)1, L"Only o1 should remain on s1", LINE_INFO());
      Assert::AreEqual(s2.get().size(), (size_t)2, L"o2 and o3 should be on s2", LINE_INFO());
      o1 = std::move(o3);
      Assert::IsTrue(s1.get().empty(), L"Nothing should remain on s1 after move", LINE_INFO());
      Assert::AreEqual(s2.get().size(), (size_t)3, L"All others should be on s2", LINE_INFO());
    }

    TEST_METHOD(Observer_Derived) {
      bool b = false; int i = 0; float f = 0.0f;
      using ObserverType = Observer<std::unique_ptr<ArgsStruct>>;
      ObserverType o1 = ObserverType(std::make_unique<ArgsStruct>(i, f, b));
      ObserverType o2 = ObserverType(std::make_unique<ArgsStructDerived>(i, f, b));
      ObserverType::SubjectType s;
      o1.observe(&s);
      o2.observe(&s);
      for(auto o : s.get())
        o->get()->onEvent();
      Assert::AreEqual(o1.get()->mInt, 1, L"onEvent should have set this", LINE_INFO());
      Assert::AreEqual(o2.get()->mInt, 2, L"onEvent should have set this", LINE_INFO());
    }
  };
}