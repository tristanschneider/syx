#include "Precompile.h"
#include "CppUnitTest.h"

#include "lua/LuaTypeInfo.h"
#include "lua/LuaSerializer.h"
#include "lua/LuaState.h"

struct Basic {
  std::string a;
};

struct Two {
  int a;
  bool b;
};

struct Composite {
  int self;
  Two t;
};

namespace ecx {
  template<>
  struct StaticTypeInfo<Basic> : StructTypeInfo<StaticTypeInfo<Basic>
    , ecx::AutoTypeList<&Basic::a>
    , ecx::AutoTypeList<>
    > {
    inline static const std::array<std::string, 1> MemberNames = { "a" };
  };

  template<>
  struct StaticTypeInfo<Two> : StructTypeInfo<StaticTypeInfo<Two>
    , ecx::AutoTypeList<&Two::a, &Two::b>
    , ecx::AutoTypeList<>
    > {
    inline static const std::array<std::string, 2> MemberNames = { "a", "b" };
  };

  template<>
  struct StaticTypeInfo<Composite> : StructTypeInfo<StaticTypeInfo<Composite>
    , ecx::AutoTypeList<&Composite::self, &Composite::t>
    , ecx::AutoTypeList<>
  > {
    inline static const std::array<std::string, 2> MemberNames = { "self", "t" };
  };
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LuaTests {
  TEST_CLASS(LuaTypeInfoTests) {
    std::string serializeTop(Lua::State& s) {
      Lua::Serializer serializer("", "", 3);
      std::string result;
      serializer.serializeTop(s.get(), result);
      return result;
    }

    TEST_METHOD(LuaTypeInfoBasic_Push_MemberPushed) {
      Basic b{ "test" };
      Lua::State state;

      Lua::LuaTypeInfo<Basic>::push(state.get(), b);

      std::string serialized = serializeTop(state);
      Assert::AreEqual(std::string("{a = \"test\"}"), serialized);
    }

    TEST_METHOD(LuaTypeInfoBasic_PushRead_ValuePreserved) {
      Basic b{ "test" };
      Lua::State state;
      Lua::LuaTypeInfo<Basic>::push(state.get(), b);

      Basic restored = Lua::LuaTypeInfo<Basic>::fromTop(state.get());

      Assert::AreEqual(b.a, restored.a);
    }

    TEST_METHOD(LuaTypeInfoTwo_Push_MembersPushed) {
      Two t{ 1, true };
      Lua::State state;

      Lua::LuaTypeInfo<Two>::push(state.get(), t);

      std::string serialized = serializeTop(state);
      Assert::AreEqual(std::string("{a = 1,b = true}"), serialized);
    }

    TEST_METHOD(LuaTypeInfoComposite_Push_MembersPushedRecursively) {
      Composite c;
      c.self = 1;
      c.t.a = 2;
      c.t.b = "value";
      Lua::State state;

      Lua::LuaTypeInfo<Composite>::push(state.get(), c);

      std::string serialized = serializeTop(state);
      Assert::AreEqual(std::string("{self = 1,t = {a = 2,b = true}}"), serialized);
    }

    TEST_METHOD(LuaTypeInfoComposite_PushRead_ValuesPreserved) {
      Composite c;
      c.self = 1;
      c.t.a = 2;
      c.t.b = "value";
      Lua::State state;
      Lua::LuaTypeInfo<Composite>::push(state.get(), c);

      Composite restored = Lua::LuaTypeInfo<Composite>::fromTop(state.get());

      Assert::AreEqual(c.self, restored.self);
      Assert::AreEqual(c.t.a, restored.t.a);
      Assert::AreEqual(c.t.b, restored.t.b);
    }
  };
}