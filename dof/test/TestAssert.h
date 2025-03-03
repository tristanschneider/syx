#pragma once

#include "CppUnitTest.h"

inline void assertEq(const glm::vec4& a, const glm::vec4& b, float e = 0.0001f) {
  using namespace Microsoft::VisualStudio::CppUnitTestFramework;
  Assert::AreEqual(a.x, b.x, e);
  Assert::AreEqual(a.y, b.y, e);
  Assert::AreEqual(a.z, b.z, e);
  Assert::AreEqual(a.w, b.w, e);
}

inline void assertEq(const glm::vec3& a, const glm::vec3& b, float e = 0.0001f) {
  using namespace Microsoft::VisualStudio::CppUnitTestFramework;
  Assert::AreEqual(a.x, b.x, e);
  Assert::AreEqual(a.y, b.y, e);
  Assert::AreEqual(a.z, b.z, e);
}

inline void assertEq(const glm::vec2& a, const glm::vec2& b, float e = 0.0001f) {
  using namespace Microsoft::VisualStudio::CppUnitTestFramework;
  Assert::AreEqual(a.x, b.x, e);
  Assert::AreEqual(a.y, b.y, e);
}