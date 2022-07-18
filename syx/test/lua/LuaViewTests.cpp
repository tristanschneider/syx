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

    template<class T>
    static T& checkUserdata(lua_State* l, int index) {
      if(!lua_islightuserdata(l, index)) {
        luaL_error(l, "Unexpected type, should be light userdata");
      }
      void* data = lua_touserdata(l, index);
      auto* checker = (ISafeLightUserdata*)data;
      if(!checker || checker->getType() != ecx::typeId<T, ISafeLightUserdata>()) {
        luaL_error(l, "Unexpected light userdata type");
      }
      return *(T*)data;
    }
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
      return ecx::typeId<std::decay_t<T>, ecx::LinearEntity>();
    }

    //For the purposes of argument validation say this is an IViewableComponent, not the derived type
    ecx::typeId_t<ISafeLightUserdata> getType() const override {
      return ecx::typeId<IViewableComponent, ISafeLightUserdata>();
    }
  };

  //Wrapper for the sake of type safety when validating lua arguments
  struct LuaRuntimeView : SafeLightUserdata<LuaRuntimeView> {
    LuaRuntimeView(Engine::EntityRegistry& registry, ecx::ViewedTypes types)
      : mView(registry, std::move(types)) {
    }
    Engine::RuntimeView mView;
  };

  struct LuaViewIterator : SafeLightUserdata<LuaViewIterator> {
    LuaViewIterator(const Engine::RuntimeView::It& it)
      : mIterator(it) {
    }

    Engine::RuntimeView::It mIterator;
  };

  struct IComponentProperty : ISafeLightUserdata {
    virtual ~IComponentProperty() = default;

    virtual int getValue(lua_State* l, LuaViewIterator& it) const = 0;
    virtual int setValue(lua_State* l, LuaViewIterator& it) const = 0;
  };

  template<auto>
  struct ComponentProperty {};
  template<class ComponentT, class MemberT, MemberT ComponentT::*Ptr>
  struct ComponentProperty<Ptr> : IComponentProperty {
    using MemberTy = std::decay_t<MemberT>;
    using ComponentTy = std::decay_t<ComponentT>;

    ecx::typeId_t<ISafeLightUserdata> getType() const override {
      return ecx::typeId<IComponentProperty, ISafeLightUserdata>();
    }

    int getValue(lua_State* l, LuaViewIterator& it) const override {
      if(const auto* result = it.mIterator.tryGet<const ComponentTy>()) {
       Lua::LuaTypeInfo<MemberTy>::push(l, result->*Ptr);
      }
      else {
        luaL_error(l, "Invalid component type for View in getValue");
      }
      return 1;
    }

    int setValue(lua_State* l, LuaViewIterator& it) const override {
      if(auto* result = it.mIterator.tryGet<ComponentTy>()) {
        if(auto toSet = Lua::LuaTypeInfo<MemberTy>::fromTop(l)) {
          result->*Ptr = std::move(*toSet);
          return 0;
        }
        else {
          luaL_error(l, "Invalid property value type for setValue");
        }
      }
      else {
        luaL_error(l, "Invalid component type for View in setValue");
      }
      return 0;
    }
  };

  template<class T>
  struct PropertyReflector {
    using TypeInfoT = ecx::StaticTypeInfo<T>;

    template<auto Ptr>
    static constexpr const IComponentProperty& _getProperty() {
      static ComponentProperty<Ptr> singleton;
      return singleton;
    }

    template<auto... M>
    static constexpr auto _membersToProperties(ecx::AutoTypeList<M...>) {
      return ecx::AutoTypeList<&_getProperty<M>...>{};
    }

    static auto getMemberNames() {
      std::array<std::string, ecx::typeListSize(typename TypeInfoT::MemberTypeList{})> result;
      for(size_t i = 0; i < result.size(); ++i) {
        result[i] = TypeInfoT::getMemberName(i);
      }
      return result;
    }

    //AutoTypeList of function pointers that return IComponentProperty objects
    inline static constexpr auto MemberProperties = _membersToProperties(TypeInfoT::MembersList);
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
    //TODO: use frame allocator or something, these are only needed during a system tick and can be discarded afterwards
    std::vector<std::unique_ptr<LuaViewIterator>> mIterators;
    Engine::EntityRegistry* mRegistry = nullptr;
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

  //The interface to view related functions, does not represent an instance of a view
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
      LuaSystemContext::ViewInfo info{ LuaRuntimeView{ *context.mRegistry, std::move(types) } };
      auto view = std::make_unique<LuaSystemContext::ViewInfo>(std::move(info));
      lua_pushlightuserdata(l, &view->mView);
      context.mViews.push_back(std::move(view));
      return 1;
    }

    static LuaViewIterator* begin(LuaRuntimeView& view, lua_State* l) {
      return _returnResultIterator(view, view.mView.begin(), l);
    }

    //TODO: iterator safety checks
    static LuaViewIterator* find(LuaRuntimeView& view, LuaViewIterator& toFind, lua_State* l) {
      return _returnResultIterator(view, view.mView.find(toFind.mIterator.entity()), l);
    }

    static LuaViewIterator* _returnResultIterator(LuaRuntimeView& view, const Engine::RuntimeView::It& it, lua_State* l) {
      if(it == view.mView.end()) {
        return nullptr;
      }
      auto& context = LuaSystemContext::get(l);
      auto storedIt = std::make_unique<LuaViewIterator>(it);
      LuaViewIterator* result = storedIt.get();
      context.mIterators.push_back(std::move(storedIt));
      return result;
    }

    // ResultType getValue(iterator, property)
    static int getValue(lua_State* l) {
      LuaViewIterator& it = ISafeLightUserdata::checkUserdata<LuaViewIterator>(l, 1);
      IComponentProperty& property = ISafeLightUserdata::checkUserdata<IComponentProperty>(l, 2);
      return property.getValue(l, it);
    }

    // void setValue(iterator, property, value)
    static int setValue(lua_State* l) {
      LuaViewIterator& it = ISafeLightUserdata::checkUserdata<LuaViewIterator>(l, 1);
      IComponentProperty& property = ISafeLightUserdata::checkUserdata<IComponentProperty>(l, 2);
      //Third parameter is the top of the stack which is what this will set from
      return property.setValue(l, it);
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

  template<size_t Size, class... Rest>
  std::array<std::string, Size + sizeof...(Rest)> combineNames(const std::array<std::string, Size>& l, Rest&&... r) {
    std::array<std::string, Size + sizeof...(Rest)> result;
    size_t i = 0;
    for(const std::string& s : l) {
      result[i++] = s;
    }
    ((result[i++] = std::forward<Rest>(r)), ...);
    return result;
  }

  template<class T>
  struct StaticTypeInfo<LuaViewableComponent<T>> : StructTypeInfo<StaticTypeInfo<LuaViewableComponent<T>>
    , ecx::AutoTypeList<>
    , decltype(ecx::combine(PropertyReflector<T>::MemberProperties, ecx::AutoTypeList<
        &LuaViewableComponent<T>::read
      , &LuaViewableComponent<T>::write
      , &LuaViewableComponent<T>::include
      , &LuaViewableComponent<T>::exclude>{}))
  > {
    inline static constexpr const char* SelfName = StaticTypeInfo<T>::SelfName;
    inline static const std::array FunctionNames = combineNames(PropertyReflector<T>::getMemberNames(),
      "read",
      "write",
      "include",
      "exclude"
    );
  };

  template<>
  struct StaticTypeInfo<LuaView> : StructTypeInfo<StaticTypeInfo<LuaView>
    , ecx::AutoTypeList<>
    , ecx::AutoTypeList<
      &LuaView::create,
      &LuaView::begin,
      &LuaView::find,
      &LuaView::getValue,
      &LuaView::setValue
    >
  > {
    inline static constexpr const char* SelfName = "View";
    inline static const std::array FunctionNames = {
      std::string("create"),
      std::string("begin"),
      std::string("find"),
      std::string("getValue"),
      std::string("setValue")
    };
  };

  template<>
  struct StaticTypeInfo<LuaViewIterator> : StructTypeInfo<StaticTypeInfo<LuaViewIterator>
    , ecx::AutoTypeList<>, ecx::AutoTypeList<>, ecx::TypeList<Lua::BindReference>> {};
  template<>
  struct StaticTypeInfo<LuaRuntimeView> : StructTypeInfo<StaticTypeInfo<LuaRuntimeView>
    , ecx::AutoTypeList<>, ecx::AutoTypeList<>, ecx::TypeList<Lua::BindReference>> {};
}

namespace Lua {
  //These are singletons, so forward them around as light userdata (pointers)
  template<class T>
  struct LuaSafeLightUserdataTypeInfoImpl {
    static int push(lua_State* l, const T& value) {
      lua_pushlightuserdata(l, (void*)&value);
      return 1;
    }

    static std::optional<T*> fromTop(lua_State* l) {
      if(lua_islightuserdata(l, -1)) {
        void* data = lua_touserdata(l, -1);
        const auto* checker = (const ISafeLightUserdata*)data;
        if(checker && checker->getType() == ecx::typeId<T, ISafeLightUserdata>()) {
          return std::make_optional((T*)data);
        }
      }
      luaL_error(l, "Unexpected type");
      return {};
    }
  };

  template<>
  struct LuaTypeInfo<IViewableComponent> : LuaSafeLightUserdataTypeInfoImpl<IViewableComponent> {};
  template<>
  struct LuaTypeInfo<IComponentProperty> : LuaSafeLightUserdataTypeInfoImpl<IComponentProperty> {};
  template<>
  struct LuaTypeInfo<LuaViewIterator> : LuaSafeLightUserdataTypeInfoImpl<LuaViewIterator> {};
  template<>
  struct LuaTypeInfo<LuaRuntimeView> : LuaSafeLightUserdataTypeInfoImpl<LuaRuntimeView> {};
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
    local valueAKey = DemoComponentA.mValue();
    local valueBKey = DemoCOmponentB.mB();

    -- Iterate over each entity in the view
    -- It's an iterator in the view, so it's only valid for accessing values in the view
    for it in View.each(w) do
      -- Values can be accessed by these keys, to which only keys contained in the view will work
      local value = View.getValue(it, valueAKey);
      View.setValue(it, valueAKey, value + 1);
    end

    local entityInR = View.begin(r);
    -- An iterator from one view can be used to get another view's iterator of the same entity with `find`
    local entityInB = View.find(b, entityInR);
    -- That iterator can then be used the same as before to access values
    View.setValue(entityInB, valueBKey, 2);
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
    using DemoB = LuaViewableComponent<DemoComponentB>;
    using DemoBBinder = Lua::LuaBinder<DemoB>;
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

    TEST_METHOD(Component_GetPropertyAccessor_Executes) {
      Lua::State s;
      DemoABinder::openLib(s.get());
      assertScriptExecutes(s.get(), "local t = DemoComponentA.mValue()");
    }

    struct LuaStateWithContext {
      LuaStateWithContext() {
        mContext.mRegistry = &mRegistry;
        mGenerator = mRegistry.createEntityGenerator();
        LuaSystemContext::set(mState.get(), mContext);
        ViewBinder::openLib(mState.get());
        DemoABinder::openLib(mState.get());
        DemoBBinder::openLib(mState.get());
      }

      lua_State* operator&() {
        return mState.get();
      }

      Lua::State mState;
      Engine::EntityRegistry mRegistry;
      LuaSystemContext mContext;
      std::shared_ptr<ecx::IndependentEntityGenerator> mGenerator;
    };

    TEST_METHOD(View_Create_Executes) {
      LuaStateWithContext s;
      assertScriptExecutes(&s, "local t = View.create(DemoComponentA.read())");
    }

    TEST_METHOD(EmptyView_Begin_IsNil) {
      LuaStateWithContext s;
      assertScriptExecutes(&s, "assert(View.begin(View.create(DemoComponentA.read())) == nil)");
    }

    TEST_METHOD(View_Begin_Executes) {
      LuaStateWithContext s;
      s.mRegistry.createEntityWithComponents<DemoComponentA>(*s.mGenerator);
      assertScriptExecutes(&s, "assert(View.begin(View.create(DemoComponentA.read())) ~= nil)");
    }

    TEST_METHOD(View_GetValue_ReturnsValue) {
      LuaStateWithContext s;
      auto&& [entity, component] = s.mRegistry.createAndGetEntityWithComponents<DemoComponentA>(*s.mGenerator);
      component.get().mValue = 2;
      assertScriptExecutes(&s, "assert(View.getValue(View.begin(View.create(DemoComponentA.read())), DemoComponentA.mValue()) == 2)");
    }

    TEST_METHOD(View_SetValue_IsSet) {
      LuaStateWithContext s;
      auto&& [entity, component] = s.mRegistry.createAndGetEntityWithComponents<DemoComponentA>(*s.mGenerator);

      assertScriptExecutes(&s, "View.setValue(View.begin(View.create(DemoComponentA.write())), DemoComponentA.mValue(), 2)");

      Assert::AreEqual(2, component.get().mValue);
    }

    TEST_METHOD(View_Find_IsFound) {
      LuaStateWithContext s;
      auto&& [entity, a, b] = s.mRegistry.createAndGetEntityWithComponents<DemoComponentA, DemoComponentB>(*s.mGenerator);

      assertScriptExecutes(&s, "assert(View.find(View.create(DemoComponentB.read()), View.begin(View.create(DemoComponentA.read()))) ~= nil)");
    }
  };
}