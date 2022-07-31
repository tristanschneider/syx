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
    //Returns a void* that can be cast to the given type if possible, otherwise nullptr
    //Presumably returns `this` but allows for composition if desired
    virtual void* tryCast(const ecx::typeId_t<ISafeLightUserdata>& destinationType) = 0;

    template<class T>
    static T& checkUserdata(lua_State* l, int index) {
      if(!lua_islightuserdata(l, index)) {
        luaL_error(l, "Unexpected type, should be light userdata");
      }
      void* data = lua_touserdata(l, index);
      auto* checker = (ISafeLightUserdata*)data;
      data = checker ? checker->tryCast(ecx::typeId<T, ISafeLightUserdata>()) : nullptr;
      if(!data) {
        luaL_error(l, "Unexpected light userdata type");
      }
      return *(T*)data;
    }
  };

  template<class... SelfT>
  struct SafeLightUserdata : ISafeLightUserdata {
    template<class Current, class... Rest>
    void* _tryCastOne(const ecx::typeId_t<ISafeLightUserdata>& destinationType) {
      //Try this one
      if(ecx::typeId<Current, ISafeLightUserdata>() == destinationType) {
        return this;
      }
      //Try the next, if there are any
      if constexpr(sizeof...(Rest) > 0) {
        return _tryCastOne<Rest...>(destinationType);
      }
      //None left, return null
      else {
        return nullptr;
      }
    }

    void* tryCast(const ecx::typeId_t<ISafeLightUserdata>& destinationType) override {
      return _tryCastOne<SelfT...>(destinationType);
    }
  };

  struct LuaRuntimeCommandBuffer : SafeLightUserdata<LuaRuntimeCommandBuffer> {
    LuaRuntimeCommandBuffer(ecx::CommandBuffer<ecx::LinearEntity>& buffer)
      : mBuffer(buffer, {}) {
    }
    Engine::RuntimeCommandBuffer mBuffer;
  };

  //Scripts create these and pass them to the view constructor to indicate the desired component and access
  struct IViewableComponent : ISafeLightUserdata {
    using TypeIdT = ecx::ViewedTypes::TypeId;
    virtual ~IViewableComponent() = default;
    virtual ViewableAccessType getAccessType() const = 0;
    virtual TypeIdT getViewedType() const = 0;
    virtual void addComponent(LuaRuntimeCommandBuffer& cmd, const ecx::LinearEntity& entity) const = 0;
    virtual void removeComponent(LuaRuntimeCommandBuffer& cmd, const ecx::LinearEntity& entity) const = 0;
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
    void* tryCast(const ecx::typeId_t<ISafeLightUserdata>& destinationType) override {
      return destinationType == ecx::typeId<IViewableComponent, ISafeLightUserdata>() ? this : nullptr;
    }

    void addComponent(LuaRuntimeCommandBuffer& cmd, const ecx::LinearEntity& entity) const override {
      cmd.mBuffer.addComponent<T>(entity);
    }

    void removeComponent(LuaRuntimeCommandBuffer& cmd, const ecx::LinearEntity& entity) const override {
      cmd.mBuffer.removeComponent<T>(entity);
    }
  };

  struct ILuaEntity : ISafeLightUserdata {
    virtual Engine::Entity getEntity() const = 0;
  };

  //Wrapper for the sake of type safety when validating lua arguments
  struct LuaRuntimeView : SafeLightUserdata<LuaRuntimeView> {
    LuaRuntimeView(Engine::EntityRegistry& registry, ecx::ViewedTypes types)
      : mView(registry, std::move(types)) {
    }

    Engine::RuntimeView mView;
  };

  struct LuaViewIterator : ILuaEntity {
    LuaViewIterator(const Engine::RuntimeView::It& it)
      : mIterator(it) {
    }

    void* tryCast(const ecx::typeId_t<ISafeLightUserdata>& destinationType) override {
      return destinationType == ecx::typeId<LuaViewIterator, ISafeLightUserdata>() ||
        destinationType == ecx::typeId<ILuaEntity, ISafeLightUserdata>() ? this : nullptr;
    }

    Engine::Entity getEntity() const override {
      return mIterator.entity();
    }

    Engine::RuntimeView::It mIterator;
    //Goofy hack to make the first _iterate call no-op
    bool mNeedsInit = true;
  };

  //An entity created through the command buffer that doesn't fully exist yet
  struct LuaPendingEntity : ILuaEntity {
    void* tryCast(const ecx::typeId_t<ISafeLightUserdata>& destinationType) override {
      return destinationType == ecx::typeId<ILuaEntity, ISafeLightUserdata>() ? this : nullptr;
    }

    Engine::Entity getEntity() const override {
      return mEntity;
    }

    Engine::Entity mEntity;
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

    void* tryCast(const ecx::typeId_t<ISafeLightUserdata>& destinationType) override {
      return destinationType == ecx::typeId<IComponentProperty, ISafeLightUserdata>() ? this : nullptr;
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
      constexpr size_t memberCount = ecx::typeListSize(typename TypeInfoT::MemberTypeList{});
      std::array<std::string, memberCount> result;
      if constexpr(memberCount > 0) {
        for(size_t i = 0; i < result.size(); ++i) {
          result[i] = TypeInfoT::getMemberName(i);
        }
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
    std::vector<std::unique_ptr<LuaPendingEntity>> mPendingEntities;
    Engine::EntityRegistry* mRegistry = nullptr;
    ecx::CommandBuffer<ecx::LinearEntity>* mInternalCommandBuffer = nullptr;
    std::optional<LuaRuntimeCommandBuffer> mCmd;
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

  //Interface for command buffer related functions, does not represent and instance of a command buffer
  struct LuaCommandBuffer {
    static int create(lua_State* l) {
      int argCount = lua_gettop(l);
      LuaSystemContext& context = LuaSystemContext::get(l);
      //Create the command buffer if it doesn't exist already. This does mean if the call fails it still results
      //in creating a command buffer with no types, but at that point the script execution should stop so it doesn't matter
      if(!context.mCmd) {
        context.mCmd.emplace(*context.mInternalCommandBuffer);
      }

      //Internally only a single command buffer is used with the combined permissions of all requested types
      //Append these types to the existing (or newly created) command buffer
      //TODO: is this memory leaked if a lua error jumps out of here?
      ecx::CommandBufferTypes types = context.mCmd->mBuffer.getAllowedTypes();
      for(int i = 1; i <= argCount; ++i) {
        IViewableComponent& component = ISafeLightUserdata::checkUserdata<IViewableComponent>(l, i);
        const auto type = component.getViewedType();
        //Special case for destroy tag, otherwise add type to the list
        //The list will eliminate duplicates if applicable in `setAllowedTypes`
        if(type == ecx::typeId<ecx::EntityDestroyTag, ecx::LinearEntity>()) {
          types.mAllowDestroyEntity = true;
        }
        else {
          types.mTypes.push_back(component.getViewedType());
        }
      }

      context.mCmd->mBuffer.setAllowedTypes(std::move(types));

      lua_pushlightuserdata(l, &*context.mCmd);
      return 1;
    }

    // local entity = CommandBuffer.createEntityWithComponents(cmd, TypeA.write()...);
    static int createEntityWithComponents(lua_State* l) {
      int argCount = lua_gettop(l);
      if(argCount < 1) {
        luaL_error(l, "Expected at least one argument");
      }

      LuaRuntimeCommandBuffer& cmd = ISafeLightUserdata::checkUserdata<LuaRuntimeCommandBuffer>(l, 1);
      auto entity = cmd.mBuffer.createEntity();
      //TODO: what happens to entity if there's a type error?
      for(int i = 2; i <= argCount; ++i) {
        ISafeLightUserdata::checkUserdata<IViewableComponent>(l, i).addComponent(cmd, entity);
      }

      LuaSystemContext& context = LuaSystemContext::get(l);
      LuaPendingEntity p;
      p.mEntity = entity;
      context.mPendingEntities.push_back(std::make_unique<LuaPendingEntity>(p));
      lua_pushlightuserdata(l, context.mPendingEntities.back().get());
      return 1;
    }

    //CommandBuffer.addComponents(cmd, entity, TypeA.write()...)
    static int addComponents(lua_State* l) {
      const int argCount = lua_gettop(l);
      if(argCount < 3) {
        luaL_error(l, "Expected at least 3 arguments");
      }

      LuaRuntimeCommandBuffer& cmd = ISafeLightUserdata::checkUserdata<LuaRuntimeCommandBuffer>(l, 1);
      ILuaEntity& entity = ISafeLightUserdata::checkUserdata<ILuaEntity>(l, 2);
      const Engine::Entity e = entity.getEntity();
      for(int i = 3; i <= argCount; ++i) {
        ISafeLightUserdata::checkUserdata<IViewableComponent>(l, i).addComponent(cmd, e);
      }

      return 0;
    }

    //CommandBuffer.removeComponents(cmd, entity, TypeA.write())
    static int removeComponents(lua_State* l) {
      const int argCount = lua_gettop(l);
      if(argCount < 3) {
        luaL_error(l, "Expected at least 3 arguments");
      }

      LuaRuntimeCommandBuffer& cmd = ISafeLightUserdata::checkUserdata<LuaRuntimeCommandBuffer>(l, 1);
      ILuaEntity& entity = ISafeLightUserdata::checkUserdata<ILuaEntity>(l, 2);
      const Engine::Entity e = entity.getEntity();
      for(int i = 3; i <= argCount; ++i) {
        ISafeLightUserdata::checkUserdata<IViewableComponent>(l, i).removeComponent(cmd, e);
      }

      return 0;
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
        IViewableComponent& viewable = ISafeLightUserdata::checkUserdata<IViewableComponent>(l, i);
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

    // for it in View.each(view) do end
    static int each(lua_State* l) {
      LuaRuntimeView& view = ISafeLightUserdata::checkUserdata<LuaRuntimeView>(l, 1);
      //From the Lua Reference:
      //> The loop starts by evaluating explist to produce four values:
      //> an iterator function, a state, an initial value for the control variable, and a closing value.
      lua_pushcfunction(l, &_iterate);
      //No state, everything is in the iterator
      lua_pushnil(l);
      lua_pushlightuserdata(l, begin(view, l));
      return 3;
    }

    //From the Lua Reference:
    //>Then, at each iteration, Lua calls the iterator function with two arguments:
    //>the state and the control variable. The results from this call are then assigned to the loop variables, following the rules of multiple assignments
    static int _iterate(lua_State* l) {
      LuaViewIterator& it = ISafeLightUserdata::checkUserdata<LuaViewIterator>(l, 2);
      //First call comes straight from the initialization function, do nothing
      if(it.mNeedsInit) {
        it.mNeedsInit = false;
      }
      else {
        ++it.mIterator;
      }

      if(it.mIterator.isValid()) {
        //If it's still valid, return the passed in iterator that has been advanced
        lua_pushlightuserdata(l, &it);
      }
      else {
        //Otherwise iteration is over, return nil to end iteration
        lua_pushnil(l);
      }
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
  template<>
  struct StaticTypeInfo<ecx::EntityDestroyTag> : StructTypeInfo<StaticTypeInfo<ecx::EntityDestroyTag>
    , ecx::AutoTypeList<>
    , ecx::AutoTypeList<>
  > {
    inline static constexpr const char* SelfName = "EntityDestroyTag";
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
      &LuaView::each,
      &LuaView::getValue,
      &LuaView::setValue
    >
  > {
    inline static constexpr const char* SelfName = "View";
    inline static const std::array FunctionNames = {
      std::string("create"),
      std::string("begin"),
      std::string("find"),
      std::string("each"),
      std::string("getValue"),
      std::string("setValue")
    };
  };

  template<>
  struct StaticTypeInfo<LuaCommandBuffer> : StructTypeInfo<StaticTypeInfo<LuaCommandBuffer>
    , ecx::AutoTypeList<>
    , ecx::AutoTypeList<
      &LuaCommandBuffer::create,
      &LuaCommandBuffer::createEntityWithComponents,
      &LuaCommandBuffer::addComponents,
      &LuaCommandBuffer::removeComponents
    >
  > {
    inline static constexpr const char* SelfName = "CommandBuffer";
    inline static const std::array FunctionNames = {
      std::string("create"),
      std::string("createEntityWithComponents"),
      std::string("addComponents"),
      std::string("removeComponents")
    };
  };

  template<>
  struct StaticTypeInfo<LuaViewIterator> : StructTypeInfo<StaticTypeInfo<LuaViewIterator>
    , ecx::AutoTypeList<>, ecx::AutoTypeList<>, ecx::TypeList<Lua::BindReference>> {};
  template<>
  struct StaticTypeInfo<LuaRuntimeView> : StructTypeInfo<StaticTypeInfo<LuaRuntimeView>
    , ecx::AutoTypeList<>, ecx::AutoTypeList<>, ecx::TypeList<Lua::BindReference>> {};
  template<>
  struct StaticTypeInfo<LuaPendingEntity> : StructTypeInfo<StaticTypeInfo<LuaPendingEntity>
    , ecx::AutoTypeList<>, ecx::AutoTypeList<>, ecx::TypeList<Lua::BindReference>> {};
  template<>
  struct StaticTypeInfo<LuaRuntimeCommandBuffer> : StructTypeInfo<StaticTypeInfo<LuaRuntimeCommandBuffer>
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
        auto* checker = (ISafeLightUserdata*)data;
        data = checker ? checker->tryCast(ecx::typeId<T, ISafeLightUserdata>()) : nullptr;
        if(data) {
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
  template<>
  struct LuaTypeInfo<LuaRuntimeCommandBuffer> : LuaSafeLightUserdataTypeInfoImpl<LuaRuntimeCommandBuffer> {};
}

namespace {
  const char* VIEW_DEMO = R"(
    local r = View.create(DemoComponentA.read());
    local w = View.create(DemoComponentA.write());
    local i = View.create(DemoComponentA.include());
    local e = View.create(DemoComponentA.include(), DemoComponentB.exclude());
    local b = View.create(DemoComponentB.write());
    local valueAKey = DemoComponentA.mValue();
    local valueBKey = DemoComponentB.mB();

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

  const char* CMD_DEMO = R"(
    -- Command buffer is created using the same type information from views. In this case the access type doesn't matter
    -- This command buffer is then allowed to create entities, and add or remove components of type DemoComponentA
    local cmd = CommandBuffer.create(DemoComponentA.write(), DemoComponentB.write());
    -- Special type to grant the command buffer permission to remove entities. Removal is special because
    -- removing an entity could remove components of any type since it's whatever that entity had
    local rem = CommandBuffer.create(EntityDestroyTag.write());

    local e = CommandBuffer.createEntityWithComponents(cmd, DemoComponentA.write(), DemoComponentB.write());
    -- To set values of a new entity, the command buffer functino must be used because the command processing is deferred
    -- so at this point the only thing that knows about the new entity is the command buffer itself
    -- No getValue is exposed this way and this is only intended for use with entities created from the command buffer
    CommandBuffer.setValue(cmd, DemoComponentA.mValue(), 5);
    -- Components can be added or removed
    CommandBuffer.addComponents(cmd, e, DemoComponentA.write());
    CommandBuffer.removeComponents(cmd, e, DemoComponentA.write());
    -- Destruction requires a command buffer created from `EntityDestroyTag`
    CommandBuffer.destroyEntity(rem, e);
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
    using CommandBinder = Lua::LuaBinder<LuaCommandBuffer>;

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
      LuaStateWithContext()
        : mInternalBuffer(mRegistry) {
        mContext.mRegistry = &mRegistry;
        mGenerator = mRegistry.createEntityGenerator();
        mContext.mInternalCommandBuffer = &mInternalBuffer;
        LuaSystemContext::set(mState.get(), mContext);
        ViewBinder::openLib(mState.get());
        DemoABinder::openLib(mState.get());
        DemoBBinder::openLib(mState.get());
        CommandBinder::openLib(mState.get());
        Lua::LuaBinder<LuaViewableComponent<ecx::EntityDestroyTag>>::openLib(mState.get());
      }

      lua_State* operator&() {
        return mState.get();
      }

      Lua::State mState;
      Engine::EntityRegistry mRegistry;
      LuaSystemContext mContext;
      std::shared_ptr<ecx::IndependentEntityGenerator> mGenerator;
      ecx::CommandBuffer<ecx::LinearEntity> mInternalBuffer;
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

    TEST_METHOD(View_Each_Iterates) {
      LuaStateWithContext s;
      auto&& [entity, a] = s.mRegistry.createAndGetEntityWithComponents<DemoComponentA>(*s.mGenerator);
      a.get().mValue = 7;

      assertScriptExecutes(&s, R"(
        local view = View.create(DemoComponentA.read());
        local count = 0;
        for entity in View.each(view) do
          assert(View.getValue(entity, DemoComponentA.mValue()) == 7, "Component value should match");
          count = count + 1;
        end
        assert(count == 1, "Components should have been found " .. count);
      )");
    }

    TEST_METHOD(CommandBuffer_Create_Executes) {
      LuaStateWithContext s;

      assertScriptExecutes(&s, "local c = CommandBuffer.create(DemoComponentA.write(), EntityDestroyTag.write())");

      Assert::IsTrue(s.mContext.mCmd.has_value());
      Assert::AreEqual(size_t(1), s.mContext.mCmd->mBuffer.getAllowedTypes().mTypes.size());
      Assert::AreEqual(ecx::typeId<DemoComponentA, ecx::LinearEntity>().mId, s.mContext.mCmd->mBuffer.getAllowedTypes().mTypes.front().mId);
      Assert::IsTrue(s.mContext.mCmd->mBuffer.getAllowedTypes().mAllowDestroyEntity);
    }

    TEST_METHOD(CommandBuffer_CreateEntityWithComponents_Executes) {
      LuaStateWithContext s;

      assertScriptExecutes(&s, R"(local c = CommandBuffer.create(DemoComponentA.write());
        local e = CommandBuffer.createEntityWithComponents(c, DemoComponentA.write());
      )");
      s.mContext.mInternalCommandBuffer->processAllCommands(s.mRegistry);

      Assert::AreEqual(size_t(1), s.mRegistry.size<DemoComponentA>());
    }

    TEST_METHOD(CommandBuffer_CreateEmptyEntity_EntityExists) {
      LuaStateWithContext s;
      const size_t preSize = s.mRegistry.size();

      assertScriptExecutes(&s, R"(local c = CommandBuffer.create();
        local e = CommandBuffer.createEntityWithComponents(c);
      )");
      s.mContext.mInternalCommandBuffer->processAllCommands(s.mRegistry);

      Assert::AreEqual(preSize + 1, s.mRegistry.size());
    }

    TEST_METHOD(CommandBuffer_AddComponentToCreatedEntity_HasComponents) {
      LuaStateWithContext s;

      assertScriptExecutes(&s, R"(local c = CommandBuffer.create(DemoComponentA.write(), DemoComponentB.write());
        local e = CommandBuffer.createEntityWithComponents(c, DemoComponentA.write());
        CommandBuffer.addComponents(c, e, DemoComponentB.write());
      )");
      s.mContext.mInternalCommandBuffer->processAllCommands(s.mRegistry);

      Assert::AreEqual(size_t(1), s.mRegistry.size<DemoComponentA>());
      Assert::AreEqual(size_t(1), s.mRegistry.size<DemoComponentB>());
    }

    TEST_METHOD(CommandBuffer_AddComponentToExistingEntity_HasComponents) {
      LuaStateWithContext s;
      auto entity = s.mRegistry.createEntityWithComponents<DemoComponentA>(*s.mGenerator);

      assertScriptExecutes(&s, R"(local c = CommandBuffer.create(DemoComponentB.write());
        local e = View.begin(View.create(DemoComponentA.include()));
        CommandBuffer.addComponents(c, e, DemoComponentB.write());
      )");
      s.mContext.mInternalCommandBuffer->processAllCommands(s.mRegistry);

      Assert::IsTrue(s.mRegistry.hasComponent<DemoComponentB>(entity));
    }

    TEST_METHOD(CommandBuffer_RemoveComponentFromCreatedEntity_IsRemoved) {
      LuaStateWithContext s;

      assertScriptExecutes(&s, R"(local c = CommandBuffer.create(DemoComponentA.write(), DemoComponentB.write());
        local e = CommandBuffer.createEntityWithComponents(c, DemoComponentA.write(), DemoComponentB.write());
        CommandBuffer.removeComponents(c, e, DemoComponentB.write());
      )");
      s.mContext.mInternalCommandBuffer->processAllCommands(s.mRegistry);

      Assert::AreEqual(size_t(1), s.mRegistry.size<DemoComponentA>());
      Assert::AreEqual(size_t(0), s.mRegistry.size<DemoComponentB>());
    }

    TEST_METHOD(CommandBuffer_RemoveComponentFromExistingEntity_IsRemoved) {
      LuaStateWithContext s;
      auto entity = s.mRegistry.createEntityWithComponents<DemoComponentA>(*s.mGenerator);

      assertScriptExecutes(&s, R"(local c = CommandBuffer.create(DemoComponentA.write());
        local e = View.begin(View.create(DemoComponentA.include()));
        CommandBuffer.removeComponents(c, e, DemoComponentA.write());
      )");
      s.mContext.mInternalCommandBuffer->processAllCommands(s.mRegistry);

      Assert::IsFalse(s.mRegistry.hasComponent<DemoComponentA>(entity));
    }
  };
}