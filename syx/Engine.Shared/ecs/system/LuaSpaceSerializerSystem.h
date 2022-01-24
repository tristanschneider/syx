#pragma once

#include "ecs/system/SpaceSerializerSystem.h"

#include "lua/AllLuaLibs.h"
#include "lua/LuaNode.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/LuaSerializer.h"
#include "lua/LuaTypeInfo.h"
#include "lua.hpp"

template<class ComponentT>
struct LuaComponentSerialize {
  using Components = std::vector<std::pair<Engine::Entity, ComponentT>>;
  using Buffer = std::vector<uint8_t>;

  static Buffer serialize(const Components& components) {
    Lua::State state;
    Lua::AllLuaLibs libs;
    libs.open(state.get());
    Buffer result;

    lua_createtable(state.get(), 0, 2);
    for(const std::pair<Engine::Entity, ComponentT>& pair : components) {
      //Write entity to "e"
      lua_pushinteger(state.get(), static_cast<LUA_INTEGER>(pair.first.mData.mRawId));
      lua_setfield(state.get(), -2, "e");

      //Write properties to "p"
      Lua::LuaTypeInfo<ComponentT>::push(state.get(), pair.second);
      lua_setfield(state.get(), -2, "p");
    }

    Lua::Serializer serializer("  ", "\n", 3);
    std::string buffer;
    serializer.serializeTop(state.get(), buffer);
    result.resize(buffer.size());
    std::memcpy(result.data(), buffer.data(), buffer.size());

    return result;
  }

  static Components deserialize(const Buffer&) {
    //TODO:
    return {};
  }
};