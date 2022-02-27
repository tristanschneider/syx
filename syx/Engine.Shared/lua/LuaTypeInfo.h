#pragma once

#include "lua.hpp"
#include "lua/LuaStackAssert.h"
#include <optional>
#include "SyxMat4.h"
#include "TypeInfo.h"

//Uses ecx::TypeInfo to provide LuaTypeInfo<T> which can be used to read or write to lua
//Types can specialize to expose their lua functionality
namespace Lua {
  struct BindReference {};

  template<class C, class Enabled = void>
  struct IsReferenceBound : std::false_type {};
  template<class C>
  struct IsReferenceBound<C, std::enable_if_t<ecx::StaticTypeInfo<C>::template HasTagsT<Lua::BindReference>::value>> : std::true_type {};

  template<class T, class Enabled = void>
  struct LuaTypeInfo {
    static_assert(sizeof(T) == -1, "Should have specialized");
    static int push(lua_State* l, const T& value);

    static std::optional<T> fromTop(lua_State* l);
  };

  template<>
  struct LuaTypeInfo<bool> {
    static int push(lua_State* l, bool value) {
      lua_pushboolean(l, value);
      return 1;
    }

    static std::optional<bool> fromTop(lua_State* l) {
      return lua_isboolean(l, -1) ? std::make_optional(static_cast<bool>(lua_toboolean(l, -1))) : std::nullopt;
    }
  };

  template<>
  struct LuaTypeInfo<std::string> {
    static int push(lua_State* l, const std::string& value) {
      lua_pushstring(l, value.c_str());
      return 1;
    }

    static std::optional<std::string> fromTop(lua_State* l) {
      return lua_isstring(l, -1) ? std::make_optional(std::string(lua_tostring(l, -1))) : std::nullopt;
    }
  };

  template<class T>
  struct LuaTypeInfo<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static int push(lua_State* l, const T& value) {
      lua_pushnumber(l, static_cast<lua_Number>(value));
      return 1;
    }

    static std::optional<T> fromTop(lua_State* l) {
      return lua_isnumber(l, -1) ? std::make_optional(static_cast<T>(lua_tonumber(l, -1))) : std::nullopt;
    }
  };

  template<class T>
  struct LuaTypeInfo<T, std::enable_if_t<std::is_integral_v<T>>> {
    static int push(lua_State* l, const T& value) {
      lua_pushinteger(l, static_cast<lua_Integer>(value));
      return 1;
    }

    static std::optional<T> fromTop(lua_State* l) {
      return lua_isinteger(l, -1) ? std::make_optional(static_cast<T>(lua_tointeger(l, -1))) : std::nullopt;
    }
  };

  template<class T>
  struct PtrTypeInfo {
    static int push(lua_State* l, const T* value) {
      if(!value) {
        lua_pushnil(l);
      }
      else {
        LuaTypeInfo<T>::push(l, *value);
      }
      return 1;
    }

    //Outer optional indicates if the type was valid, inner optional indicate if the value was present
    static std::optional<std::optional<T>> fromTop(lua_State* l) {
      if(lua_isnil(l, -1)) {
        return std::make_optional(std::optional<T>());
      }
      if(auto result = LuaTypeInfo<T>::fromTop(l)) {
        return std::make_optional(result);
      }
      return std::nullopt;
    }
  };

  template<class T>
  struct LuaTypeInfo<T*, void> : PtrTypeInfo<std::decay_t<T>> {
  };

  template<class T>
  struct LuaTypeInfo<const T*, void> : PtrTypeInfo<std::decay_t<T>> {
  };

  template<>
  struct LuaTypeInfo<Syx::Mat4> {
    static int push(lua_State* l, const Syx::Mat4& m) {
      lua_createtable(l, 16, 0);
      for(int r = 0; r < 4; ++r) {
        for(int c = 0; c < 4; ++c) {
          lua_pushinteger(l, (r * 4) + c + 1);
          lua_pushnumber(l, m[c][r]);
          lua_settable(l, -3);
        }
      }
      return 1;
    }

    static std::optional<Syx::Mat4> fromTop(lua_State* l) {
      if(!lua_istable(l, -1)) {
        return {};
      }
      Syx::Mat4 m;
      //The visual layout is [c0.x, c1.x, c2.x, c3.x] but the data layout is [c0.x, c0.y, c0.z, c0.w] so have columns in the inner loop
      for(int r = 0; r < 4; ++r) {
        for(int c = 0; c < 4; ++c) {
          lua_pushinteger(l, (r * 4) + c + 1);
          if(lua_gettable(l, -2) != LUA_TNUMBER) {
            lua_pop(l, 1);
            return {};
          }
          m[c][r] = static_cast<float>(lua_tonumber(l, -1));
          lua_pop(l, 1);
        }
      }
      return std::make_optional(m);
    }
  };

  //TODO: might need to be able to do both reference and copy
  template<class T>
  struct LuaTypeInfo<T, std::enable_if_t<ecx::StaticTypeInfo<T>::template HasTagsT<Lua::BindReference>::value>> {
    static int push(lua_State* l, const T& value) {
      auto binding = (const T**)lua_newuserdata(l, sizeof(T*));
      *binding = &value;
      luaL_setmetatable(l, ecx::StaticTypeInfo<T>::getTypeName().c_str());
      return 1;
    }

    static std::optional<T*> fromTop(lua_State* l) {
      void* result = luaL_testudata(l, -1, ecx::StaticTypeInfo<T>::getTypeName().c_str());
      return result ? std::make_optional(*static_cast<T**>(result)) : std::nullopt;
    }
  };

  template<class T>
  struct LuaTypeInfo<T, std::enable_if_t<(ecx::StaticTypeInfo<T>::MemberCount > 0) && std::is_same_v<ecx::TypeList<>, typename ecx::StaticTypeInfo<T>::TagsList>>> {
    static int push(lua_State* l, const T& value) {
      Lua::StackAssert a(l, 1);
      lua_newtable(l);

      ecx::StaticTypeInfo<T>::visitShallow([l](const std::string& name, const auto& value) {
        //Field name
        lua_pushstring(l, name.c_str());
        //Push value for field by recursing into the LuaTypeInfo for this type
        LuaTypeInfo<std::decay_t<decltype(value)>>::push(l, value);
        //Put the key/value into the table
        lua_settable(l, -3);
      }, value);
      return 1;
    }

    static std::optional<T> fromTop(lua_State* l) {
      Lua::StackAssert a(l);
      T result;
      bool allFound = true;
      ecx::StaticTypeInfo<T>::visitShallow([l, &allFound](const std::string& name, auto& value) {
        //If the value was found, get the type info for this member and use it to read the value
        bool foundThis = false;
        if(lua_getfield(l, -1, name.c_str()) != LUA_TNONE) {
          if(auto top = LuaTypeInfo<std::decay_t<decltype(value)>>::fromTop(l)) {
            value = std::move(*top);
            foundThis = true;
          }
          lua_pop(l, 1);
        }
        allFound = allFound && foundThis;
      }, result);

      return result;
    }
  };

  //Exposes openLib which creates the registry entry for all functions declared by ecx::TypeInfo<T>
  //Limitations:
  //- Pointer/reference types only allowed on argument types and only for reference bound arguments
  //- No by-value returns of BindReference types
  //- Reference types for primitives won't change the caller's value (lua)
  template<class T>
  struct LuaBinder {
    //Creates a `callNative` method which uses ecx::TypeInfo to pull the arguments from the lua stack and call the wrapped method
    template<class FunctionInfoT>
    struct FunctionBinder {
      template<class A, class Enabled = void>
      struct Applier {
        static_assert(sizeof(A) == -1, "Should have specialized one of the below");
      };

      template<class T>
      static T _checkError(lua_State* l, std::optional<T>&& value) {
        if(value) {
          return std::move(*value);
        }
        luaL_error(l, "Invalid method argument");
        //lua error won't return
        return *value;
      }

      //Common implementation for value types
      template<class A>
      struct ApplyArgumentByValue {
        static A applyArgument(lua_State* l, size_t index) {
          lua_pushvalue(l, static_cast<int>(index));
          A result = _checkError(l, LuaTypeInfo<std::decay_t<A>>::fromTop(l));
          lua_pop(l, 1);
          return result;
        }
      };

      template<class A>
      struct ApplyReferenceBoundArgumentByPointer {
        static A applyArgument(lua_State* l, size_t index) {
          //Allow nil as nullptr
          if(lua_isnil(l, static_cast<int>(index))) {
            return nullptr;
          }
          lua_pushvalue(l, static_cast<int>(index));
          A result = _checkError(l, LuaTypeInfo<std::decay_t<std::remove_pointer_t<A>>>::fromTop(l));
          lua_pop(l, 1);
          return result;
        }
      };

      template<class A>
      using IsByValueT = std::is_same<A, std::decay_t<std::remove_pointer_t<A>>>;

      //Reference bound types by value
      template<class A>
      struct Applier<A, std::enable_if_t<!IsReferenceBound<A>::value && IsByValueT<A>::value>> : ApplyArgumentByValue<A> {
      };

      //Reference bound types by const value
      template<class A>
      struct Applier<const A, std::enable_if_t<!IsReferenceBound<A>::value>> : ApplyArgumentByValue<A> {
      };

      //Same as above for reference types that should be copied
      template<class A>
      struct Applier<A, std::enable_if_t<IsReferenceBound<A>::value>> {
        static A applyArgument(lua_State* l, size_t index) {
          lua_pushvalue(l, static_cast<int>(index));
          A result = *_checkError(l, LuaTypeInfo<std::decay_t<A>>::fromTop(l));
          lua_pop(l, 1);
          return result;
        }
      };

      //Pointer arguments enabled for reference types
      template<class A>
      struct Applier<A*, std::enable_if_t<IsReferenceBound<A>::value>> : ApplyReferenceBoundArgumentByPointer<A*> {
      };

      //Const pointer arguments enabled for reference types
      template<class A>
      struct Applier<const A*, std::enable_if_t<IsReferenceBound<A>::value>> : ApplyReferenceBoundArgumentByPointer<const A*> {
      };

      //Reference arguments enabled for reference types
      template<class A>
      struct Applier<A&, std::enable_if_t<IsReferenceBound<A>::value>> {
        static A& applyArgument(lua_State* l, size_t index) {
          lua_pushvalue(l, static_cast<int>(index));
          A& result = *_checkError(l, LuaTypeInfo<std::decay_t<A>>::fromTop(l));
          lua_pop(l, 1);
          return result;
        }
      };

      //const reference arguments enabled for reference types
      template<class A>
      struct Applier<const A&, std::enable_if_t<IsReferenceBound<A>::value>> {
        static const A& applyArgument(lua_State* l, size_t index) {
          lua_pushvalue(l, static_cast<int>(index));
          const A& result = *_checkError(l, LuaTypeInfo<std::decay_t<A>>::fromTop(l));
          lua_pop(l, 1);
          return result;
        }
      };

      template<class A>
      // Explicit return to preserve references, type list to allow overload resolution of template type
      static decltype(LuaTypeInfo<std::decay_t<A>>::fromTop(nullptr)) applyArgument(lua_State* l, ecx::TypeList<A>, size_t index) {
        lua_pushvalue(l, static_cast<int>(index));
        auto result = _checkError(l, LuaTypeInfo<std::decay_t<A>>::fromTop(l));
        lua_pop(l, 1);
        return result;
      }

      template<class ReturnT, class... Args, size_t... Indices>
      static ReturnT _applyArguments(lua_State* l, ecx::TypeList<Args...>, std::index_sequence<Indices...>) {
        if constexpr(!std::is_same_v<void, ReturnT>) {
          //Pull each value down from lua then use that to call the function. +1 to convert from 0 based index to lua stack index starting at 1
          return FunctionInfoT::invoker().invoke(Applier<Args>::applyArgument(l, Indices + 1)...);
        }
        else {
          FunctionInfoT::invoker().invoke(Applier<Args>::applyArgument(l, Indices + 1)...);
        }
      }

      using ArgsList = typename FunctionInfoT::ArgsTypeList;

      template<class ReturnT>
      static int _callNative(lua_State* l, ecx::TypeList<ReturnT>) {
        if constexpr(std::is_pointer_v<ReturnT>) {
          if(ReturnT ptr = _applyArguments<ReturnT>(l, ArgsList{}, std::make_index_sequence<ecx::typeListSize(ArgsList{})>())) {
            return LuaTypeInfo<std::decay_t<std::remove_pointer_t<ReturnT>>>::push(l, *ptr);
          }
          lua_pushnil(l);
          return 1;
        }
        else {
          return LuaTypeInfo<std::decay_t<ReturnT>>::push(l,
            _applyArguments<ReturnT>(l, ArgsList{}, std::make_index_sequence<ecx::typeListSize(ArgsList{})>()));
        }
      }

      static int _callNative(lua_State* l, ecx::TypeList<void>) {
        _applyArguments<void>(l, typename FunctionInfoT::ArgsTypeList{}, std::make_index_sequence<ecx::typeListSize(ArgsList{})>());
        return 0;
      }

      static int callNative(lua_State* l) {
        return _callNative(l, ecx::TypeList<typename FunctionInfoT::ReturnT>{});
      };
    };

    template<bool MemberFns, size_t I>
    static void _bindLibEntry(luaL_Reg*& entry) {
      using FunctionInfoT = decltype(ecx::StaticTypeInfo<T>::getFunctionInfo<I>());
      if constexpr(FunctionInfoT::IsMemberFn == MemberFns) {
        entry->name = ecx::StaticTypeInfo<T>::getFunctionName<I>().c_str();
        entry->func = &FunctionBinder<FunctionInfoT>::callNative;
        ++entry;
      }
    }

    template<bool MemberFns, size_t FnCount, size_t... Indices>
    static void _bindLibEntries(std::array<luaL_Reg, FnCount>& entries, std::index_sequence<Indices...>) {
      luaL_Reg* currentEntry = &entries[0];
      (_bindLibEntry<MemberFns, Indices>(currentEntry), ...);
    }

    static void openLib(lua_State* l) {
      //Make a full array for all functions, only some of them will be used
      constexpr size_t functionCount = ecx::StaticTypeInfo<T>::getFunctionCount();
      std::array<luaL_Reg, functionCount> members{ { nullptr, nullptr } };
      std::array<luaL_Reg, functionCount> statics{ { nullptr, nullptr } };
      _bindLibEntries<true>(members, std::make_index_sequence<functionCount>());
      _bindLibEntries<false>(statics, std::make_index_sequence<functionCount>());

      Lua::StackAssert sa(l);
      const std::string& selfName = ecx::StaticTypeInfo<T>::getTypeName();
      luaL_newmetatable(l, selfName.c_str());
      luaL_setfuncs(l, members.data(), 0);
      lua_pushvalue(l, -1);
      lua_setfield(l, -2, "__index");
      lua_pop(l, 1);
      luaL_newlib(l, statics.data());
      lua_setglobal(l, selfName.c_str());
    }
  };

  //Call a method on the top of the stack. Returns optional<Return> or bool if void is provided
  template<class Return, class... Args>
  auto tryCallMethod(lua_State* l, const Args&... args) {
    const int argCount = sizeof...(args);
    //Expecting to pop the function from the stack
    Lua::StackAssert sa(l, -1);
    (LuaTypeInfo<std::decay_t<Args>>::push(l, args), ...);

    if constexpr(std::is_same_v<void, Return>) {
      if(int error = lua_pcall(l, argCount, 0, 0)) {
        lua_pop(l, 1);
        return false;
      }
      return true;
    }
    else {
      if(int error = lua_pcall(l, argCount, 1, 0)) {
        lua_pop(l, 1);
        return decltype(LuaTypeInfo<Return>::fromTop(l)){};
      }
      auto result = LuaTypeInfo<Return>::fromTop(l);
      lua_pop(l, 1);
      return result;
    }
  }

  template<class T>
  auto returnEmptyResult() {
    if constexpr(std::is_same_v<void, T>) {
      return false;
    }
    else {
      return decltype(LuaTypeInfo<T>::fromTop(nullptr)){};
    }
  }

  template<class Return, class... Args>
  auto tryCallGlobalMethod(lua_State* l, const char* name, const Args&... args) {
    StackAssert sa(l);
    if(lua_getglobal(l, name) == LUA_TFUNCTION) {
      return tryCallMethod<Return>(l, args...);
    }
    return returnEmptyResult<Return>();
  }
};