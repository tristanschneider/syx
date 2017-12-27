#include "Precompile.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include <atomic>
#include <algorithm>
#include <thread>
#include <mutex>
#include "threading/RWLock.h"

namespace LockTest {
  TEST_CLASS(RWLockReadBasic){
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
      sleepMS(15);
      Assert::AreEqual(value, 2, L"Value should have been written by other thread by now", LINE_INFO());
      rw.readLock();
      rw.readUnlock();
      writer.join();
    }
  };
}