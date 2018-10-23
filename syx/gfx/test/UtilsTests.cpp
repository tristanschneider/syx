#include "Precompile.h"
#include "test/TestRegistry.h"

#include "util/Variant.h"
#include "util/ScratchPad.h"

TEST_FUNC(Variant_SetGetInt_ValuePreserved) {
  Variant v;
  v.set(5);
  TEST_ASSERT(v.get<int>() == 5, "should match set value");
  TEST_ASSERT(v.getType() == Variant::Type::Int, "should be int");
}

TEST_FUNC(Variant_SetGetFloat_ValuePreserved) {
  Variant v;
  v.set(3.5f);
  TEST_ASSERT(v.get<float>() == 3.5f, "should match set value");
  TEST_ASSERT(v.getType() == Variant::Type::Float, "should be float");
}

TEST_FUNC(Variant_SetGetFloatDifferentSizes_ValuePreserved) {
  Variant v;
  v.set(3.5);
  TEST_ASSERT(v.get<float>() == 3.5f, "should match set value");
}

TEST_FUNC(Variant_SetGetString_ValuePreserved) {
  Variant v;
  v.set("string");
  TEST_ASSERT(v.get<std::string>() == "string", "should math set value");
  TEST_ASSERT(v.getType() == Variant::Type::String, "should be string");
}

TEST_FUNC(Variant_SetGetPointer_ValuePreserved) {
  Variant v;
  int i;
  v.set(&i);
  TEST_ASSERT(v.get<int*>() == &i, "void* should match and be converted to tempalted type");
  TEST_ASSERT(v.getType() == Variant::Type::VoidPtr, "should be pointer");
}

TEST_FUNC(Variant_ChangeTypes_IsNewType) {
  Variant v;
  v.set(5);
  v.set("five");
  TEST_ASSERT(v.get<std::string>() == "five", "should have string value");
  TEST_ASSERT(v.getType() == Variant::Type::String, "should have string type");
}

TEST_FUNC(ScratchPad_WriteReadValue_ValueMatches) {
  ScratchPad pad(1);
  pad.write("key", Variant(5));
  Variant* readValue = pad.read("key");
  TEST_ASSERT(readValue != nullptr, "Value should have been found");
  TEST_ASSERT(readValue->get<int>() == 5, "Value should have been preserved");
}

TEST_FUNC(ScratchPad_ReadWithRefresh_ValueIsRefreshed) {
  ScratchPad pad(1);
  pad.write("key", Variant(5));
  pad.update();
  pad.read("key");
  pad.update();
  TEST_ASSERT(pad.read("key") != nullptr, "Value should exist as the read should have refreshed its lifetime");
}

TEST_FUNC(ScratchPad_ReadWithoutRefresh_ValueIsNotRefreshed) {
  ScratchPad pad(1);
  pad.write("key", Variant(5));
  pad.update();
  pad.read("key", false);
  pad.update();
  TEST_ASSERT(pad.read("key") == nullptr, "Value should be gone from second update");
}

TEST_FUNC(ScratchPath_UpdateForLifetime_ValueIsRemoved) {
  ScratchPad pad(1);
  pad.write("key", Variant(5));
  pad.update();
  TEST_ASSERT(pad.read("key", false) != nullptr, "Value should still exist");
  pad.update();
  TEST_ASSERT(pad.read("key") == nullptr, "Value should be gone");
}

TEST_FUNC(ScratchPad_WriteOverValue_ValueIsChanged) {
  ScratchPad pad(1);
  pad.write("key", Variant(5));
  pad.write("key", Variant("five"));
  TEST_ASSERT(pad.read("key")->get<std::string>() == "five", "Should have new value");
}

TEST_FUNC(ScratchPad_WriteOverValue_LifetimeIsRefreshed) {
  ScratchPad pad(1);
  pad.write("key", Variant(5));
  pad.update();
  pad.write("key", Variant(5));
  pad.update();
  TEST_ASSERT(pad.read("key") != nullptr, "Value should exist as read should have refreshed its lifetime");
}

TEST_FUNC(ScratchPad_ReadWriteMultipleKeys_MultipleKeysExist) {
  ScratchPad pad(1);
  pad.write("a", Variant("a"));
  pad.write("b", Variant("b"));
  pad.write("c", Variant("c"));
  TEST_ASSERT(pad.read("a")->get<std::string>() == "a", "Value should exist");
  TEST_ASSERT(pad.read("b")->get<std::string>() == "b", "Value should exist");
  TEST_ASSERT(pad.read("c")->get<std::string>() == "c", "Value should exist");
}

TEST_FUNC(ScratchPad_SameValueDifferentScopes_ValuesAreDifferent) {
  ScratchPad pad(1);
  pad.push("a");
  pad.write("key", Variant("a"));
  pad.pop();

  pad.push("b");
  pad.write("key", Variant("b"));
  pad.pop();

  pad.push("a");
  TEST_ASSERT(pad.read("key")->get<std::string>() == "a", "Value in scope a should be a");
  pad.pop();

  pad.push("b");
  TEST_ASSERT(pad.read("key")->get<std::string>() == "b", "Value in scope a should be b");
  pad.pop();
}

TEST_FUNC(ScratchPad_ClearKeys_PadIsEmpty) {
  ScratchPad pad(1);
  pad.write("key", Variant(5));
  pad.clear();
  TEST_ASSERT(pad.read("key") == nullptr, "Value should have been cleared");
}

TEST_FUNC(ScratchPad_AddMultipleValuesDifferentLifetimes_ValuesDestroyedInOrder) {
  ScratchPad pad(1);
  pad.write("a", Variant("a"));
  pad.update();
  pad.write("b", Variant("b"));
  pad.update();
  TEST_ASSERT(pad.read("a") == nullptr, "a should have expired");
  pad.update();
  TEST_ASSERT(pad.read("b") == nullptr, "b should have expired");
}