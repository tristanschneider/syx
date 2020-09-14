#include "Precompile.h"
#include "CppUnitTest.h"

#include <lua.hpp>
#include "lua/LuaCache.h"
#include "lua/LuaState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LuaTests {
  TEST_CLASS(LuaCache) {
  public:
    TEST_METHOD(ScopedCache_CreateEntry_HasValue) {
      Lua::State state;
      luaL_newmetatable(state, "type");
      auto factory = std::make_shared<Lua::ScopedCacheFactory>("factory");
      std::unique_ptr<Lua::ScopedCacheInstance> instance = factory->createInstance(state);

      std::unique_ptr<Lua::ScopedCacheEntry> entry = instance->createEntry(&state, "type");
      Assert::AreEqual(entry->push(), 1, L"One value should be pushed on the stack", LINE_INFO());

      Assert::IsTrue(Lua::ScopedCacheEntry::checkParam(state, -1, "type") == &state, L"Value should match the entry it was created with", LINE_INFO());
    }

    TEST_METHOD(ScopedCache_DestroyFactoryThenUseChildren_ChildrenNoOp) {
      Lua::State state;
      luaL_newmetatable(state, "type");
      auto factory = std::make_shared<Lua::ScopedCacheFactory>("factory");
      std::unique_ptr<Lua::ScopedCacheInstance> instance = factory->createInstance(state);
      std::unique_ptr<Lua::ScopedCacheEntry> entry = instance->createEntry(&state, "type");

      factory = nullptr;

      Assert::IsTrue(instance->createEntry(nullptr, "type") == nullptr, L"Should noop since factory is gone");
      Assert::IsTrue(entry->push() == 0, L"Should noop because factory is gone", LINE_INFO());
    }
  };
}