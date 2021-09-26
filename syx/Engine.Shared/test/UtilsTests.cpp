#include "Precompile.h"
#include "test/TestRegistry.h"

#include "util/Variant.h"
#include "util/ScratchPad.h"
#include "util/Finally.h"

TEST_FUNC(ScratchPad_ReadWithRefresh_ValueIsRefreshed) {
  ScratchPad pad(1);
  pad.write("key", Variant{5});
  pad.update();
  pad.read("key");
  pad.update();
  TEST_ASSERT(pad.read("key") != nullptr, "Value should exist as the read should have refreshed its lifetime");
}

TEST_FUNC(ScratchPad_ReadWithoutRefresh_ValueIsNotRefreshed) {
  ScratchPad pad(1);
  pad.write("key", Variant{5});
  pad.update();
  pad.read("key", false);
  pad.update();
  TEST_ASSERT(pad.read("key") == nullptr, "Value should be gone from second update");
}

TEST_FUNC(ScratchPath_UpdateForLifetime_ValueIsRemoved) {
  ScratchPad pad(1);
  pad.write("key", Variant{5});
  pad.update();
  TEST_ASSERT(pad.read("key", false) != nullptr, "Value should still exist");
  pad.update();
  TEST_ASSERT(pad.read("key") == nullptr, "Value should be gone");
}

TEST_FUNC(ScratchPad_WriteOverValue_ValueIsChanged) {
  ScratchPad pad(1);
  pad.write("key", Variant{5});
  pad.write("key", Variant{std::string("five")});
  TEST_ASSERT(std::get<std::string>(pad.read("key")->mData) == "five", "Should have new value");
}

TEST_FUNC(ScratchPad_WriteOverValue_LifetimeIsRefreshed) {
  ScratchPad pad(1);
  pad.write("key", Variant{5});
  pad.update();
  pad.write("key", Variant{5});
  pad.update();
  TEST_ASSERT(pad.read("key") != nullptr, "Value should exist as read should have refreshed its lifetime");
}

TEST_FUNC(ScratchPad_ReadWriteMultipleKeys_MultipleKeysExist) {
  ScratchPad pad(1);
  pad.write("a", Variant{std::string("a")});
  pad.write("b", Variant{std::string("b")});
  pad.write("c", Variant{std::string("c")});
  TEST_ASSERT(std::get<std::string>(pad.read("a")->mData) == "a", "Value should exist");
  TEST_ASSERT(std::get<std::string>(pad.read("b")->mData) == "b", "Value should exist");
  TEST_ASSERT(std::get<std::string>(pad.read("c")->mData) == "c", "Value should exist");
}

TEST_FUNC(ScratchPad_SameValueDifferentScopes_ValuesAreDifferent) {
  ScratchPad pad(1);
  pad.push("a");
  pad.write("key", Variant{std::string("a")});
  pad.pop();

  pad.push("b");
  pad.write("key", Variant{std::string("b")});
  pad.pop();

  pad.push("a");
  TEST_ASSERT(std::get<std::string>(pad.read("key")->mData) == "a", "Value in scope a should be a");
  pad.pop();

  pad.push("b");
  TEST_ASSERT(std::get<std::string>(pad.read("key")->mData) == "b", "Value in scope a should be b");
  pad.pop();
}

TEST_FUNC(ScratchPad_ClearKeys_PadIsEmpty) {
  ScratchPad pad(1);
  pad.write("key", Variant{5});
  pad.clear();
  TEST_ASSERT(pad.read("key") == nullptr, "Value should have been cleared");
}

TEST_FUNC(ScratchPad_AddMultipleValuesDifferentLifetimes_ValuesDestroyedInOrder) {
  ScratchPad pad(1);
  pad.write("a", Variant{std::string("a")});
  pad.update();
  pad.write("b", Variant{std::string("b")});
  pad.update();
  TEST_ASSERT(pad.read("a") == nullptr, "a should have expired");
  pad.update();
  TEST_ASSERT(pad.read("b") == nullptr, "b should have expired");
}

TEST_FUNC(Finally_Construct_DoesAction) {
  int actions = 0;
  {
    auto f = finally([&actions]() { ++actions; });
  }
  TEST_ASSERT(actions == 1, "Action should have been performed once");
}

TEST_FUNC(Finally_MoveConstruct_DoesActionOnce) {
  int actions = 0;
  {
    auto a = finally([&actions]() { ++actions; });
    auto b = std::move(a);
  }
  TEST_ASSERT(actions == 1, "Action should have been performed once by moved item");
}

TEST_FUNC(Finally_Cancel_DoesNoAction) {
  int actions = 0;
  {
    auto f = finally([&actions]() { ++actions; });
    f.cancel();
  }
  TEST_ASSERT(actions == 0, "Action should have been cancelled");
}