#include "Precompile.h"
#include "CppUnitTest.h"

#include "generics/HashMap.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(HashMapTest) {
    TEST_METHOD(Basic) {
      gnx::HashMap<int, std::string> map;
      map[5] = "value";
      auto it = map.find(5);
      Assert::IsTrue(it != map.end());
      Assert::AreEqual(5, it->first);
      Assert::AreEqual(std::string("value"), it->second);
      Assert::AreEqual(size_t(1), map.size());
      map.erase(it);
      Assert::IsTrue(map.empty());

      auto [inserted, wasNew] = map.insert(std::make_pair(int{ 1 }, std::string{ "one" }));
      auto [dup, dupWasNew] = map.insert(std::make_pair(int{ 1 }, std::string{ "two" }));
      Assert::IsTrue(wasNew);
      Assert::IsFalse(dupWasNew);
      Assert::IsTrue(inserted == dup);
      Assert::AreEqual(1, inserted->first);
      Assert::AreEqual(std::string{ "one" }, inserted->second);

      map.clear();
      Assert::IsTrue(map.empty());

      map.reserve(100);
      for(size_t i = 0; i < 100; ++i) {
        map[static_cast<int>(i)] = std::to_string(i);
      }
      gnx::HashMap<int, std::string> copyCtor{ map };
      gnx::HashMap<int, std::string> copyAssign;
      copyAssign = copyCtor;
      auto temp = copyAssign;
      gnx::HashMap<int, std::string> moveCtor{ std::move(temp) };
      temp = moveCtor;
      gnx::HashMap<int, std::string> moveAssign;
      moveAssign = std::move(temp);

      Assert::AreEqual(size_t(100), map.size());
      for(auto&& [k, v] : map) {
        Assert::AreEqual(std::to_string(k), v);
      }
      for(auto&& [k, v] : copyCtor) {
        Assert::AreEqual(std::to_string(k), v);
      }
      for(auto&& [k, v] : copyAssign) {
        Assert::AreEqual(std::to_string(k), v);
      }
      for(auto&& [k, v] : moveCtor) {
        Assert::AreEqual(std::to_string(k), v);
      }
      for(auto&& [k, v] : moveAssign) {
        Assert::AreEqual(std::to_string(k), v);
      }

      auto r = map.begin();
      while(r != map.end()) {
        map.erase(r);
      }
      Assert::IsTrue(map.empty());
    }
  };
}