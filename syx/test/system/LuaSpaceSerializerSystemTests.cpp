#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/system/LuaSpaceSerializerSystem.h"

#include "component/Transform.h"
#include "TypeInfo.h"

#include "lua.hpp"

struct TestComponent {
  bool operator==(const TestComponent& rhs) const {
    return value == rhs.value;
  }

  int value = 0;
};

namespace ecx {
  template<>
  struct StaticTypeInfo<TestComponent> : StructTypeInfo<StaticTypeInfo<TestComponent>
    , ecx::AutoTypeList<&TestComponent::value>
    , ecx::AutoTypeList<>
    > {
    inline static const std::array<std::string, 1> MemberNames = { "value" };
  };
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SystemTests {
  using namespace Engine;

  TEST_CLASS(LuaSpaceSerializerSystemTests) {
    void assertBufferMatches(const std::vector<uint8_t>& buffer, const std::string expected) {
      Assert::AreEqual(expected, std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
    }

    TEST_METHOD(LuaComponentSerialize_SerializeSingleComponent_IsSerialized) {
      using Serializer = LuaComponentSerialize<TestComponent>;
      Serializer::Components components;
      components.push_back(std::make_pair(Entity(0), TestComponent{1}));

      std::vector<uint8_t> buffer = Serializer::serialize(components);
      assertBufferMatches(buffer,
R"({
  {
    e = 0,
    p = {
      value = 1
    }
  }
})");
    }

    TEST_METHOD(LuaComponentSerialize_SingleComponentRoundTrip_Matches) {
      using Serializer = LuaComponentSerialize<TestComponent>;
      Serializer::Components components;
      components.push_back(std::make_pair(Entity(0), TestComponent{1}));

      Serializer::Components result = Serializer::deserialize(Serializer::serialize(components));

      Assert::IsTrue(components == result);
    }

    TEST_METHOD(LuaComponentSerialize_SerializeTwoComponents_AreSerialized) {
      using Serializer = LuaComponentSerialize<TestComponent>;
      Serializer::Components components;
      components.push_back(std::make_pair(Entity(0), TestComponent{1}));
      components.push_back(std::make_pair(Entity(99), TestComponent{2}));

      std::vector<uint8_t> buffer = Serializer::serialize(components);

      assertBufferMatches(buffer,
R"({
  {
    e = 0,
    p = {
      value = 1
    }
  },
  {
    e = 99,
    p = {
      value = 2
    }
  }
})");
    }

    TEST_METHOD(LuaComponentSerialize_TwoComponentsRoundTrip_Matches) {
      using Serializer = LuaComponentSerialize<TestComponent>;
      Serializer::Components components;
      components.push_back(std::make_pair(Entity(50), TestComponent{11}));
      components.push_back(std::make_pair(Entity(12), TestComponent{13}));

      Serializer::Components result = Serializer::deserialize(Serializer::serialize(components));

      Assert::IsTrue(components == result);
    }

    TEST_METHOD(LuaComponentSerialize_MaxEntityRoundTrip_Matches) {
      using Serializer = LuaComponentSerialize<TestComponent>;
      Serializer::Components components;
      components.push_back(std::make_pair(Entity(std::numeric_limits<uint32_t>().max(), 0), TestComponent{11}));
      components.push_back(std::make_pair(Entity(std::numeric_limits<uint32_t>().max() - 1, 0), TestComponent{11}));
      Serializer::Components result = Serializer::deserialize(Serializer::serialize(components));

      Assert::IsTrue(components == result);
    }
  };
}