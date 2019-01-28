#include "Precompile.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include <atomic>
#include <algorithm>
#include "util/Observer.h"

namespace ObserverTests {
  struct ArgsStruct : public Observer<ArgsStruct> {
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

  struct EmptyObserver : public Observer<EmptyObserver> {
  };

  TEST_CLASS(ObserverTest) {
  public:
    TEST_METHOD(Observer_SubjectOutlive) {
      EmptyObserver::SubjectType s;
      {
        EmptyObserver o;
        o.observe(&s);
        Assert::IsTrue(s.get().front() == &o);
        Assert::IsTrue(o.hasSubject());
      }
      Assert::IsTrue(s.get().empty(), L"Observer should have been removed upon destruction", LINE_INFO());
    }

    TEST_METHOD(Observer_ObserverOutlive) {
      EmptyObserver o;
      {
        EmptyObserver::SubjectType s;
        o.observe(&s);
        Assert::IsTrue(s.get().front() == &o);
        Assert::IsTrue(o.hasSubject());
      }
      Assert::IsFalse(o.hasSubject(), L"Subject should have removed itself from observer upon destruction", LINE_INFO());
    }

    TEST_METHOD(Observer_DoubleAdd) {
      EmptyObserver::SubjectType s;
      EmptyObserver o;
      o.observe(&s);
      o.observe(&s);
      Assert::AreEqual(s.get().size(), (size_t)1, L"Duplicate observation should have been prevented", LINE_INFO());
      Assert::IsTrue(o.hasSubject());
    }

    TEST_METHOD(Observer_Derived) {
      bool b = false; int i = 0; float f = 0.0f;
      ArgsStruct o1(i, f, b);
      ArgsStructDerived o2(i, f, b);
      ArgsStruct::SubjectType s;
      o1.observe(&s);
      o2.observe(&s);
      for(auto o : s.get())
        o->onEvent();
      Assert::AreEqual(o1.mInt, 1, L"onEvent should have set this", LINE_INFO());
      Assert::AreEqual(o2.mInt, 2, L"onEvent should have set this", LINE_INFO());
    }
  };
}