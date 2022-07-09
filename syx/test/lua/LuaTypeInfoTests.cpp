#include "Precompile.h"
#include "CppUnitTest.h"

#include "lua/LuaTypeInfo.h"
#include "lua/LuaSerializer.h"
#include "lua/LuaState.h"
#include "Util.h"

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

struct WithMethods {
  int value = 0;

  void incrementOne() {
    ++value;
  }

  void increment(int i) {
    value += i;
  }

  int getValue() const {
    return value;
  }

  int getAdded(const WithMethods& rhs) const {
    return value + rhs.value;
  }

  void subtractFrom(WithMethods& rhs) const {
    rhs.value -= value;
  }

  int getAddedPtr(const WithMethods* rhs) const {
    if(rhs) {
      return value + rhs->value;
    }
    return value;
  }

  void subtractFromPtr(WithMethods* rhs) const {
    if(rhs) {
      rhs->value -= value;
    }
  }

  void tryAddToThis(const WithMethods* rhs) {
    if(rhs) {
      value += rhs->value;
    }
  }

  WithMethods& identity() {
    return *this;
  }

  const WithMethods& constIdentity() const {
    return *this;
  }

  WithMethods* getPtr() {
    return this;
  }

  const WithMethods* constGetPtr() const {
    return this;
  }

  static int get7() {
    return 7;
  }

  static void intMethod(int) {
  }

  WithMethods* getNothing() const {
    return nullptr;
  }
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

  template<>
  struct StaticTypeInfo<WithMethods> : StructTypeInfo<StaticTypeInfo<WithMethods>
    , ecx::TypeList<>
    , ecx::TypeList<
        ecx::TaggedType<&WithMethods::increment>,
        ecx::TaggedType<&WithMethods::incrementOne>,
        ecx::TaggedType<&WithMethods::get7>,
        ecx::TaggedType<&WithMethods::getAdded>,
        ecx::TaggedType<&WithMethods::subtractFrom>,
        ecx::TaggedType<&WithMethods::getAddedPtr>,
        ecx::TaggedType<&WithMethods::subtractFromPtr>,
        ecx::TaggedType<&WithMethods::tryAddToThis>,
        ecx::TaggedType<&WithMethods::identity>,
        ecx::TaggedType<&WithMethods::constIdentity>,
        ecx::TaggedType<&WithMethods::getPtr>,
        ecx::TaggedType<&WithMethods::constGetPtr>,
        ecx::TaggedType<&WithMethods::getNothing>,
        ecx::TaggedType<&WithMethods::getValue>,
        ecx::TaggedType<&WithMethods::intMethod>
    >
    , ecx::TypeList<Lua::BindReference>
  > {
    inline static const std::array<std::string, 15> FunctionNames {
      "increment",
      "incrementOne",
      "get7",
      "getAdded",
      "subtractFrom",
      "getAddedPtr",
      "subtractFromPtr",
      "tryAddToThis",
      "identity",
      "constIdentity",
      "getPtr",
      "constGetPtr",
      "getNothing",
      "getValue",
      "intMethod"
    };
    static inline constexpr const char* SelfName = "WithMethods";
  };
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LuaTests {
  static_assert(Lua::IsReferenceBound<WithMethods>::value);
  static_assert(!Lua::IsReferenceBound<WithMethods*>::value);
  static_assert(!Lua::IsReferenceBound<WithMethods&>::value);
  static_assert(!Lua::IsReferenceBound<const WithMethods*>::value);
  static_assert(!Lua::IsReferenceBound<const WithMethods&>::value);
  static_assert(!Lua::IsReferenceBound<const WithMethods>::value);
  static_assert(!Lua::IsReferenceBound<Composite>::value);
  static_assert(!Lua::IsReferenceBound<int>::value);

  TEST_CLASS(LuaTypeInfoTests) {
    std::string serializeTop(Lua::State& s) {
      Lua::Serializer serializer("", "", 3);
      std::string result;
      serializer.serializeTop(s.get(), result);
      return result;
    }

    void callTestWithObject(lua_State* l, WithMethods& obj, const char* script) {
      Lua::LuaBinder<WithMethods>::openLib(l);
      if(!luaL_dostring(l, script)) {
        int func = lua_getglobal(l, "test");
        if(func == LUA_TFUNCTION) {
          Lua::LuaTypeInfo<WithMethods>::push(l, obj);
          if(int error = lua_pcall(l, 1, 0, 0)) {
            //Error message is on top of the stack. Display then pop it
            Assert::Fail((L"Error executing script " + Util::toWide(std::string(lua_tostring(l, -1)))).c_str(), LINE_INFO());
          }
        }
        else {
          Assert::Fail(L"test function not found", LINE_INFO());
        }
      }
      else {
        Assert::Fail((L"Script execution failed " + Util::toWide(std::string(lua_tostring(l, -1)))).c_str(), LINE_INFO());
      }
    }

    TEST_METHOD(StaticMemberFunction_CallIncorrectly_IsError) {
      Lua::State state;
      const char* script = R"(
        function test()
          WithMethods.intMethod("seven")
        end
      )";

      Lua::LuaBinder<WithMethods>::openLib(state.get());
      if(!luaL_dostring(state.get(), script)) {
        int func = lua_getglobal(state.get(), "test");
        if(func == LUA_TFUNCTION) {
          Assert::IsTrue(0 != lua_pcall(state.get(), 0, 0, 0), L"Should trigger error", LINE_INFO());
          return;
        }
      }
      Assert::Fail(L"Test authoring issue");
    }

    TEST_METHOD(StaticMemberFunction_Call_ReturnsValue) {
      Lua::State state;
      WithMethods obj;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(WithMethods.get7() == 7, "should be 7")
        end
      )");
    }

    TEST_METHOD(MemberFunctionNoValue_Call_ValueIncremented) {
      Lua::State state;
      WithMethods obj;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          obj:incrementOne()
        end
      )");

      Assert::AreEqual(1, obj.value);
    }

    TEST_METHOD(MemberFunctionWithValue_Call_ValueIncremented) {
      Lua::State state;
      WithMethods obj;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          obj:increment(3)
        end
      )");

      Assert::AreEqual(3, obj.value);
    }

    TEST_METHOD(MemberFunctionConstRefArg_Call_ValueIncremented) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(2 == obj:getAdded(obj))
        end
      )");
    }

    TEST_METHOD(MemberFunctionConstPtrArg_Call_ValueIncremented) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(2 == obj:getAddedPtr(obj))
        end
      )");
    }

    TEST_METHOD(MemberFunctionEmptyConstPtrArg_Call_ValueIncremented) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(1 == obj:getAddedPtr(nil))
        end
      )");
    }

    TEST_METHOD(MemberFunctionRefArg_Call_ValueChanged) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          obj:subtractFrom(obj)
        end
      )");

      Assert::AreEqual(0, obj.value, L"Value should have been subtracted from itself", LINE_INFO());
    }

    TEST_METHOD(MemberFunctionPtrArg_Call_ValueChanged) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          obj:subtractFromPtr(obj)
        end
      )");

      Assert::AreEqual(0, obj.value, L"Value should have been subtracted from itself", LINE_INFO());
    }

    TEST_METHOD(MemberFunctionEmptyPtrArg_Call_ValueChanged) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          obj:tryAddToThis(nil)
        end
      )");

      Assert::AreEqual(1, obj.value, L"Value should be unchanged", LINE_INFO());
    }

    TEST_METHOD(MemberFunctionRefReturn_Call_IsReturned) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(2 == obj:getAdded(obj:identity()))
        end
      )");
    }

    TEST_METHOD(MemberFunctionConstRefReturn_Call_IsReturned) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(2 == obj:getAdded(obj:constIdentity()))
        end
      )");
    }

    TEST_METHOD(MemberFunctionConstPtrReturn_Call_IsReturned) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(1 == obj:constGetPtr():getValue())
        end
      )");
    }

    TEST_METHOD(MemberFunctionPtrReturn_Call_IsReturned) {
      Lua::State state;
      WithMethods obj;
      obj.value = 1;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(1 == obj:getPtr():getValue())
        end
      )");
    }

    TEST_METHOD(MemberFunctionEmptyPtrReturn_Call_IsReturned) {
      Lua::State state;
      WithMethods obj;

      callTestWithObject(state.get(), obj, R"(
        function test(obj)
          assert(nil == obj:getNothing())
        end
      )");
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

      std::optional<Basic> restored = Lua::LuaTypeInfo<Basic>::fromTop(state.get());

      Assert::IsTrue(restored.has_value());
      Assert::AreEqual(b.a, restored->a);
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

      std::optional<Composite> restored = Lua::LuaTypeInfo<Composite>::fromTop(state.get());

      Assert::IsTrue(restored.has_value());
      Assert::AreEqual(c.self, restored->self);
      Assert::AreEqual(c.t.a, restored->t.a);
      Assert::AreEqual(c.t.b, restored->t.b);
    }

    void executedAndPushTestMethod(lua_State* l, const char* script) {
      if(!luaL_dostring(l, script)) {
        int func = lua_getglobal(l, "test");
        Assert::AreEqual(LUA_TFUNCTION, func);
      }
      else {
        Assert::Fail((L"Script execution failed " + Util::toWide(std::string(lua_tostring(l, -1)))).c_str(), LINE_INFO());
      }
    }

    TEST_METHOD(LuaEmptyMethod_tryCallMethod_ReturnsTrue) {
      Lua::State state;
      executedAndPushTestMethod(state.get(), R"(
        function test() end
      )");

      Assert::IsTrue(Lua::tryCallMethod<void>(state.get()));
    }

    TEST_METHOD(LuaReturnMethod_tryCallMethod_ReturnsValue) {
      Lua::State state;
      executedAndPushTestMethod(state.get(), R"(
        function test() return 5 end
      )");

      auto result = Lua::tryCallMethod<int>(state.get());

      Assert::IsTrue(result.has_value());
      Assert::AreEqual(5, *result);
    }

    TEST_METHOD(LuaArgMethod_tryCallMethod_ReturnsTrue) {
      Lua::State state;
      executedAndPushTestMethod(state.get(), R"(
        function test(num) assert(num == 5) end
      )");

      Assert::IsTrue(Lua::tryCallMethod<void>(state.get(), 5));
    }

    TEST_METHOD(LuaArgMethod_tryCallMethodWithNull_IsNilArg) {
      Lua::State state;
      executedAndPushTestMethod(state.get(), R"(
        function test(arg) assert(arg == nil) end
      )");

      Assert::IsTrue(Lua::tryCallMethod<void>(state.get(), static_cast<int*>(nullptr)));
    }

    TEST_METHOD(LuaOptionalReturnMethod_tryCallMethod_IsNilArg) {
      Lua::State state;
      executedAndPushTestMethod(state.get(), R"(
        function test() end
      )");

      std::optional<std::optional<int>> result = Lua::tryCallMethod<int*>(state.get(), static_cast<int*>(nullptr));

      Assert::IsTrue(result.has_value());
      Assert::IsFalse(result->has_value());
    }

    TEST_METHOD(LuaOptionalReturnMethod_tryCallMethod_ReturnsValue) {
      Lua::State state;
      executedAndPushTestMethod(state.get(), R"(
        function test() return 5 end
      )");

      std::optional<std::optional<int>> result = Lua::tryCallMethod<int*>(state.get(), static_cast<int*>(nullptr));

      Assert::IsTrue(result.has_value());
      Assert::IsTrue(result->has_value());
      Assert::AreEqual(5, **result);
    }

    TEST_METHOD(LuaEmptyMEthod_tryCallGlobalMethod_ReturnsTrue) {
      Lua::State state;
      luaL_dostring(state.get(), "function test() end");

      Assert::IsTrue(Lua::tryCallGlobalMethod<void>(state.get(), "test"));
    }
  };
}