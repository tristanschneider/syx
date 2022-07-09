#include "Precompile.h"
#include "CppUnitTest.h"

#include "lua/LuaTypeInfo.h"
#include "lua/LuaSerializer.h"
#include "lua/LuaState.h"
#include "Util.h"

namespace {
  struct DemoComponentA {
    int mValue = 0;
  };
  struct DemoComponentB {
    int mB = 1;
  };

  enum class ViewableAccessType {
    Read,
    Write,
    Include,
    Exclude
  };

  //Safe assuming all light userdata exposed uses this
  //Allows validating type of light userdata parameters
  struct ISafeLightUserdata {
    virtual ~ISafeLightUserdata() = default;
    virtual ecx::typeId_t<ISafeLightUserdata> getType() const = 0;
  };

  template<class SelfT>
  struct SafeLightUserdata : ISafeLightUserdata {
    ecx::typeId_t<ISafeLightUserdata> getType() const override {
      return ecx::typeId<SelfT, ISafeLightUserdata>();
    }
  };

  //Scripts create these and pass them to the view constructor to indicate the desired component and access
  struct IViewableComponent : ISafeLightUserdata {
    using TypeIdT = ecx::ViewedTypes::TypeId;
    virtual ~IViewableComponent() = default;
    virtual ViewableAccessType getAccessType() const = 0;
    virtual TypeIdT getViewedType() const = 0;
  };

  template<class T, ViewableAccessType A>
  struct ViewableComponent : IViewableComponent {
    static const IViewableComponent& get() {
      static const ViewableComponent<T, A> singleton;
      return singleton;
    }

    ViewableAccessType getAccessType() const override {
      return A;
    }

    TypeIdT getViewedType() const override {
      return ecx::typeId<std::decay_t<T>, ecx::DefaultTypeCategory>();
    }

    //For the purposes of argument validation say this is an IViewableComponent, not the derived type
    ecx::typeId_t<ISafeLightUserdata> getType() const override {
      return ecx::typeId<IViewableComponent, ISafeLightUserdata>();
    }
  };

  //Wrapper for the sake of type safety when validating lua arguments
  struct LuaRuntimeView : SafeLightUserdata<LuaRuntimeView> {
    LuaRuntimeView(ecx::ViewedTypes types)
      : mView(std::move(types)) {
    }
    Engine::RuntimeView mView;
  };

  //Holds the native objects that the script uses so that everything in lua can use light userdata
  struct LuaSystemContext {
    constexpr static inline const char* KEY = "lc";

    static void set(lua_State* l, LuaSystemContext& context) {
      lua_pushlightuserdata(l, &context);
      lua_setglobal(l, KEY);
    }

    static LuaSystemContext& get(lua_State* l) {
      if(lua_getglobal(l, LuaSystemContext::KEY) != LUA_TLIGHTUSERDATA) {
        luaL_error(l, "Global context must exist");
      }
      auto* result = (LuaSystemContext*)lua_touserdata(l, -1);
      lua_pop(l, 1);
      return *result;
    }

    struct ViewInfo {
      LuaRuntimeView mView;
    };

    //Needs pointer stability since they'll be passed around by pointer in lua
    std::vector<std::unique_ptr<ViewInfo>> mViews;
  };

  template<class T>
  struct LuaViewableComponent {
    static const IViewableComponent& read() {
      return ViewableComponent<T, ViewableAccessType::Read>::get();
    }
    static const IViewableComponent& write() {
      return ViewableComponent<T, ViewableAccessType::Write>::get();
    }
    static const IViewableComponent& include() {
      return ViewableComponent<T, ViewableAccessType::Include>::get();
    }
    static const IViewableComponent& exclude() {
      return ViewableComponent<T, ViewableAccessType::Exclude>::get();
    }
  };

  struct LuaView {
    //Construct a view out of the access types indicated in the argument list
    static int create(lua_State* l) {
      int argCount = lua_gettop(l);
      if(argCount <= 0) {
        luaL_error(l, "Expected at least one arg got %d", argCount);
        return 0;
      }
      LuaSystemContext& context = LuaSystemContext::get(l);

      ecx::ViewedTypes types;
      for(int i = 1; i <= argCount; ++i) {
        if(lua_islightuserdata(l, i)) {
          void* data = lua_touserdata(l, i);
          auto* checker = (ISafeLightUserdata*)data;
          if(checker && checker->getType() == ecx::typeId<IViewableComponent, ISafeLightUserdata>()) {
            auto& viewable = *(IViewableComponent*)data;
            auto type = viewable.getViewedType();
            switch(viewable.getAccessType()) {
            case ViewableAccessType::Exclude:
              types.mExcludes.push_back(type);
              break;
            case ViewableAccessType::Include:
              types.mIncludes.push_back(type);
              break;
            case ViewableAccessType::Read:
              types.mReads.push_back(type);
              break;
            case ViewableAccessType::Write:
              types.mWrites.push_back(type);
              break;
            }
          }
          else {
            luaL_error(l, "Expected viewable component type at %d", i);
          }
        }
        else {
          luaL_error(l, "Expected viewable component type at %d was type %d", i, lua_type(l, i));
        }
      }

      //Store the view in the local cache for this script and return a pointer to it
      LuaSystemContext::ViewInfo info{ LuaRuntimeView{ std::move(types) } };
      auto view = std::make_unique<LuaSystemContext::ViewInfo>(std::move(info));
      lua_pushlightuserdata(l, view.get());
      context.mViews.push_back(std::move(view));
      return 1;
    }
  };
}

namespace ecx {
  template<>
  struct StaticTypeInfo<DemoComponentA> : StructTypeInfo<StaticTypeInfo<DemoComponentA>
    , ecx::AutoTypeList<&DemoComponentA::mValue>
    , ecx::AutoTypeList<>
    > {
    inline static const std::array<std::string, 1> MemberNames = { "mValue" };
    inline static constexpr const char* SelfName = "DemoComponentA";
  };
  template<>
  struct StaticTypeInfo<DemoComponentB> : StructTypeInfo<StaticTypeInfo<DemoComponentB>
    , ecx::AutoTypeList<&DemoComponentB::mB>
    , ecx::AutoTypeList<>
    > {
    inline static const std::array<std::string, 1> MemberNames = { "mB" };
    inline static constexpr const char* SelfName = "DemoComponentB";
  };

  template<class T>
  struct StaticTypeInfo<LuaViewableComponent<T>> : StructTypeInfo<StaticTypeInfo<LuaViewableComponent<T>>
    , ecx::AutoTypeList<>
    , ecx::AutoTypeList<
        &LuaViewableComponent<T>::read
      , &LuaViewableComponent<T>::write
      , &LuaViewableComponent<T>::include
      , &LuaViewableComponent<T>::exclude>
  > {
    inline static constexpr const char* SelfName = StaticTypeInfo<T>::SelfName;
    inline static const std::array FunctionNames = {
      std::string("read"),
      std::string("write"),
      std::string("include"),
      std::string("exclude"),
    };
  };

  template<>
  struct StaticTypeInfo<LuaView> : StructTypeInfo<StaticTypeInfo<LuaView>
    , ecx::AutoTypeList<>
    , ecx::AutoTypeList
      <&LuaView::create>
  > {
    inline static constexpr const char* SelfName = "View";
    inline static const std::array FunctionNames = {
      std::string("create"),
    };
  };
}

namespace Lua {
  //These are singletons, so forward them around as light userdata (pointers)
  template<>
  struct LuaTypeInfo<IViewableComponent> {
    static int push(lua_State* l, const IViewableComponent& value) {
      lua_pushlightuserdata(l, (void*)&value);
      return 1;
    }

    static std::optional<const IViewableComponent*> fromTop(lua_State* l) {
      if(lua_islightuserdata(l, -1)) {
        void* data = lua_touserdata(l, -1);
        const auto* checker = (const ISafeLightUserdata*)data;
        if(checker && checker->getType() == ecx::typeId<IViewableComponent, ISafeLightUserdata>()) {
          return std::make_optional((const IViewableComponent*)data);
        }
      }
      luaL_error(l, "Expected ViwableComponent type");
      return {};
    }
  };
}

namespace {
  //TODO: the string identifiers needed to specify the property and particularly the class name seem expensive
  //Could do indices but that is confusing to read.
  //Perhaps the component type info could be used, like DemoComponentA.type(), or even DemoComponentA.mValue()
  const char* DEMO = R"(
    local r = View.create(DemoComponentA.read());
    local w = View.create(DemoComponentA.write());
    local i = View.create(DemoComponentA.include());
    local e = View.create(DemoComponentA.include(), DemoComponentB.exclude());
    local b = View.create(DemoComponentB.write());
    let it = View.begin(w);
    let endIt = View.end(w);

    while it ~= endIt do
      local value = View.getValue(it, "DemoComponentA", "mValue");
      View.setValue(it, "DemoComponentA", "mValue", value + 1);
    end

    it = View.begin(r);
    local itB = View.find(it, b);
    if itB ~= View.end(b) then
      View.setValue(itB, "DemoComponentB", "mB", 2)
    end
  )";
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LuaTests {
  TEST_CLASS(LuaViewTests) {
    void assertScriptExecutes(lua_State* l, const char* script) {
      if(luaL_dostring(l, script)) {
        Assert::Fail((L"Script execution failed " + Util::toWide(std::string(lua_tostring(l, -1)))).c_str(), LINE_INFO());
      }
    }

    using DemoA = LuaViewableComponent<DemoComponentA>;
    using DemoABinder = Lua::LuaBinder<DemoA>;
    using ViewBinder = Lua::LuaBinder<LuaView>;

    TEST_METHOD(ViewableComponent_Read_Executes) {
      Lua::State s;
      DemoABinder::openLib(s.get());
      assertScriptExecutes(s.get(), "local t = DemoComponentA.read()");
    }

    TEST_METHOD(ViewableComponent_Write_Executes) {
      Lua::State s;
      DemoABinder::openLib(s.get());
      assertScriptExecutes(s.get(), "local t = DemoComponentA.write()");
    }

    TEST_METHOD(ViewableComponent_Include_Executes) {
      Lua::State s;
      DemoABinder::openLib(s.get());
      assertScriptExecutes(s.get(), "local t = DemoComponentA.include()");
    }

    TEST_METHOD(ViewableComponent_Exclude_Executes) {
      Lua::State s;
      DemoABinder::openLib(s.get());
      assertScriptExecutes(s.get(), "local t = DemoComponentA.exclude()");
    }

    TEST_METHOD(View_Create_Executes) {
      Lua::State s;
      DemoABinder::openLib(s.get());
      ViewBinder::openLib(s.get());
      LuaSystemContext ctx;
      LuaSystemContext::set(s.get(), ctx);
      assertScriptExecutes(s.get(), "local t = View.create(DemoComponentA.read())");
    }
  };
}