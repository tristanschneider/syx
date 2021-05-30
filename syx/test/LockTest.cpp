#include "Precompile.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include <atomic>
#include <algorithm>
#include <thread>
#include <mutex>
#include "threading/RWLock.h"
#include "threading/SpinLock.h"
#include "threading/ThreadLocal.h"

namespace LockTest {
  TEST_CLASS(RWLockTest) {
  public:
    TEST_METHOD(SingleRead) {
      RWLock rw;
      {
        auto reader = rw.getReader();
        Assert::IsTrue(rw.tryReadLock());
        rw.readUnlock();
        Assert::IsFalse(rw.tryWriteLock());
      }
    }

    TEST_METHOD(SingleWrite) {
      RWLock rw;
      {
        auto writer = rw.getWriter();
        Assert::IsFalse(rw.tryReadLock());
        Assert::IsFalse(rw.tryWriteLock());
      }
    }

    TEST_METHOD(MultiRead) {
      RWLock rw;
      rw.readLock();
      rw.readLock();
      rw.readUnlock();
      rw.readUnlock();
      Assert::IsTrue(rw.tryWriteLock());
      rw.writeUnlock();
      Assert::IsTrue(rw.tryReadLock());
      rw.readUnlock();
    }

    static void sleepMS(int ms) {
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    static void writeAsync(RWLock& rw, int& value) {
      auto writer = rw.getWriter();
      value = 1;
      sleepMS(10);
      value = 2;
    }

    TEST_METHOD(RWTest) {
      RWLock rw;
      int value = 0;
      rw.readLock();
      std::thread writer(&writeAsync, std::ref(rw), std::ref(value));
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      Assert::AreEqual(value, 0, L"Read lock should prevent write from starting", LINE_INFO());
      rw.readUnlock();
      sleepMS(30);
      Assert::AreEqual(value, 2, L"Value should have been written by other thread by now", LINE_INFO());
      rw.readLock();
      rw.readUnlock();
      writer.join();
    }
  };

  TEST_CLASS(SpinLockTest) {
  public:
    TEST_METHOD(SpinLockBasic) {
      SpinLock s;
      {
        std::unique_lock<SpinLock> l(s);
        Assert::IsFalse(s.try_lock());
      }
    }

    static void asyncSetValue(SpinLock& l, int& value) {
      l.lock();
      ++value;
      l.unlock();
    }

    TEST_METHOD(SpinLockThreaded) {
      SpinLock s;
      s.lock();
      int value = 0;
      std::thread setter(&asyncSetValue, std::ref(s), std::ref(value));
      //Give the other thread plenty of time to try and grab the lock
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      Assert::AreEqual(value, 0, L"Other thread shouldn't have been able to get the lock", LINE_INFO());

      s.unlock();
      //Give the other thread plenty of time to finally acquire the lock and set the value
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      Assert::AreEqual(value, 1, L"Other thread should have acquired the lock and set the value", LINE_INFO());

      setter.join();
    }
  };

  TEST_CLASS(ThreadLocalTest) {
  public:
    TEST_METHOD(ThreadLocalBasic) {
      ThreadLocal<int> tl;

      int* a = &tl.get();
      std::thread otherThread([a, &tl]() {
        int* b = &tl.get();
        Assert::AreNotEqual(a, b, L"Each thread should get their own instance", LINE_INFO());
      });
      otherThread.join();
      int* c = &tl.get();
      Assert::AreEqual(a, c, L"Same thread should get same instance", LINE_INFO());
    }
  };
}