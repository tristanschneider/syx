#include "Precompile.h"
#include "CppUnitTest.h"

#include "generics/DynamicBitset.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  static_assert(gnx::bitops::bytesToContainBits(7) == 1);
  static_assert(gnx::bitops::bytesToContainBits(8) == 1);
  static_assert(gnx::bitops::bytesToContainBits(9) == 2);

  TEST_CLASS(DynamicBitsetTest) {
    struct RecordBits {
      void operator()(const size_t& i) const {
        container.push_back(i);
      }

      std::vector<size_t>& container;
    };

    TEST_METHOD(VisitSetBits) {
      std::vector<size_t> bits;
      RecordBits recorder{ bits };

      {
        uint8_t buff{};
        gnx::bitops::visitSetBits(&buff, 8, recorder);
        Assert::IsTrue(bits.empty());

        buff = 0b00000101;
        gnx::bitops::visitSetBits(&buff, 8, recorder);
        Assert::IsTrue(std::vector<size_t>{ 0, 2 } == bits);
        bits.clear();

        gnx::bitops::visitSetBits(&buff, 2, recorder);
        Assert::IsTrue(std::vector<size_t>{ 0 } == bits);
        bits.clear();
      }
      {
        uint16_t buff = 0b0010000000000010;
        gnx::bitops::visitSetBits(reinterpret_cast<uint8_t*>(&buff), 16, recorder);
        Assert::IsTrue(std::vector<size_t>{ 1, 13 } == bits);
        bits.clear();
      }
      {
        uint32_t buff = 0b10000001000000010000000100000001;
        gnx::bitops::visitSetBits(reinterpret_cast<uint8_t*>(&buff), 32, recorder);
        Assert::IsTrue(std::vector<size_t>{ 0, 8, 16, 24, 31 } == bits);
        bits.clear();
      }
      {
        uint64_t buff = 0b1000000100000001000000010000000110000001000000010000000100000001;
        gnx::bitops::visitSetBits(reinterpret_cast<uint8_t*>(&buff), 64, recorder);
        Assert::IsTrue(std::vector<size_t>{ 0, 8, 16, 24, 31, 32, 40, 48, 56, 63 } == bits);
        bits.clear();
      }
      {
        std::array<uint64_t, 3> buff{};
        buff[0] = 1;
        buff[2] = static_cast<uint64_t>(1) << 63;
        gnx::bitops::visitSetBits(reinterpret_cast<uint8_t*>(&buff), 192, recorder);
        Assert::IsTrue(std::vector<size_t>{ 0, 191 } == bits);
        bits.clear();
      }
    }

    TEST_METHOD(MemSetBits) {
      {
        uint8_t buff = 255;
        gnx::bitops::memSetBits(&buff, false, 8);
        Assert::AreEqual(static_cast<uint8_t>(0), buff);
      }
      {
        uint8_t buff = 0;
        gnx::bitops::memSetBits(&buff, true, 8);
        Assert::AreEqual(static_cast<uint8_t>(255), buff);
      }
      {
        uint8_t buff = 255;
        gnx::bitops::memSetBits(&buff, false, 7);
        Assert::AreEqual(static_cast<uint8_t>(0b10000000), buff);
      }
      {
        uint16_t buff = std::numeric_limits<uint16_t>::max();
        gnx::bitops::memSetBits(reinterpret_cast<uint8_t*>(&buff), false, 16);
        Assert::AreEqual(static_cast<uint16_t>(0), buff);
      }
      {
        uint16_t buff = std::numeric_limits<uint16_t>::max();
        gnx::bitops::memSetBits(reinterpret_cast<uint8_t*>(&buff), false, 15);
        Assert::AreEqual(static_cast<uint16_t>(1 << 15), buff);
      }
    }

    TEST_METHOD(TestAndSet) {
      gnx::DynamicBitset s;
      s.resize(10);
      Assert::AreEqual(static_cast<size_t>(10), s.size());

      s.set(5);
      Assert::IsTrue(s.test(5));
      Assert::IsTrue(static_cast<bool>(s[5]));

      s.set(5, false);
      Assert::IsFalse(s.test(5));
      Assert::IsFalse(static_cast<bool>(s[5]));

      s[5] = true;
      Assert::IsTrue(s.test(5));
      Assert::IsTrue(static_cast<bool>(s[5]));

      s[5] = false;
      Assert::IsFalse(s.test(5));
      Assert::IsFalse(static_cast<bool>(s[5]));

      Assert::IsFalse(s.test(0));
      Assert::IsFalse(s.any());

      s.setAllBits();
      for(size_t i = 0; i < 10; ++i) {
        Assert::IsTrue(s.test(i));
      }

      s.resetBits();
      for(size_t i = 0; i < 10; ++i) {
        Assert::IsFalse(s.test(i));
      }
    }

    void testMoveAndCopy(size_t size) {
      const size_t index = size/2;
      gnx::DynamicBitset a, b;
      b.resize(size);
      b.set(index);
      a = b;

      Assert::AreEqual(size, a.size());
      Assert::IsTrue(a.test(index));

      gnx::DynamicBitset c{ a };

      Assert::AreEqual(size, c.size());
      Assert::IsTrue(c.test(index));

      gnx::DynamicBitset d{ std::move(a) };

      Assert::AreEqual(size, d.size());
      Assert::IsTrue(d.test(index));

      d = std::move(d);

      Assert::AreEqual(size, d.size());
      Assert::IsTrue(d.test(index));
    }

    TEST_METHOD(MoveAndCopy) {
      //Internal storage size
      testMoveAndCopy(3);
      testMoveAndCopy(1000);
    }

    void testIterators(size_t size) {
      const size_t i = size/2;
      gnx::DynamicBitset set;
      const gnx::DynamicBitset& cset = set;
      set.resize(size);

      Assert::IsTrue(set.begin() == set.end());
      Assert::IsTrue(cset.begin() == cset.end());

      set.set(i);

      for(auto [k, v] : cset) {
        Assert::AreEqual(i, k);
        Assert::IsTrue(static_cast<bool>(v));
      }

      for(auto [k, v] : set) {
        Assert::AreEqual(i, k);
        Assert::IsTrue(static_cast<bool>(v));
        v = false;
        Assert::IsFalse(static_cast<bool>(v));
      }
      Assert::IsTrue(set.none());
    }

    TEST_METHOD(Iterators) {
      testIterators(5);
      testIterators(1000);
    }
  };
}
