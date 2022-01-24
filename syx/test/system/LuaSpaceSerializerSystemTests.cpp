#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/system/LuaSpaceSerializerSystem.h"

#include "component/Transform.h"
#include "TypeInfo.h"

#include "lua.hpp"

struct TestComponent {
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
  e = 0,
  p = {
    value = 1
  }
})");
    }
  };
}