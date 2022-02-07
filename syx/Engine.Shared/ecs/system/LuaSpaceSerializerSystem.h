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

    //Array for all components
    lua_createtable(state.get(), static_cast<int>(components.size()), 0);
    //for(const std::pair<Engine::Entity, ComponentT>& pair : components) {
    for(size_t i = 0; i < components.size(); ++i) {
      const std::pair<Engine::Entity, ComponentT>& pair = components[i];
      //Table of entity and component value
      lua_createtable(state.get(), 0, 2);
      //Write entity to "e"
      lua_pushinteger(state.get(), static_cast<LUA_INTEGER>(pair.first.mData.mParts.mEntityId));
      lua_setfield(state.get(), -2, "e");

      //Write properties to "p"
      Lua::LuaTypeInfo<ComponentT>::push(state.get(), pair.second);
      lua_setfield(state.get(), -2, "p");

      //Add the table to the array
      lua_seti(state.get(), -2, static_cast<lua_Integer>(i + 1));
    }

    Lua::Serializer serializer("  ", "\n", 3);
    std::string buffer;
    //Serialize the array of components
    serializer.serializeTop(state.get(), buffer);
    result.resize(buffer.size());
    std::memcpy(result.data(), buffer.data(), buffer.size());

    return result;
  }

  static Components deserialize(const Buffer& buffer) {
    Components result;
    Lua::State state;
    Lua::AllLuaLibs libs;
    libs.open(state.get());
    //Buffer may not be null terminated
    //TODO: encoding?
    std::string strBuffer;
    //Goofy hack to make the lua syntax work: a={ ... }
    std::string_view prefix = "a=";
    strBuffer.resize(buffer.size() + prefix.size());
    std::memcpy(strBuffer.data(), prefix.data(), prefix.size());
    std::memcpy(&strBuffer[prefix.size()], buffer.data(), buffer.size());
    //Push table on to top of stack
    if(luaL_dostring(state.get(), strBuffer.c_str()) != LUA_OK) {
      return result;
    }

    if(lua_getglobal(state.get(), "a") != LUA_TTABLE) {
      return result;
    }

    //Iterate over each entry in the table
    lua_pushnil(state.get());  /* first key */
    while(lua_next(state.get(), -2) != 0) {
      //Value on top of the stack is a table containing entity "e" and values "p"
      if(lua_getfield(state.get(), -1, "e") == LUA_TNUMBER) {
        Engine::Entity entity(static_cast<uint32_t>(lua_tonumber(state.get(), -1)), uint32_t(0));
        if(lua_getfield(state.get(), -2, "p") == LUA_TTABLE) {
          //TODO: what if the read value is garbage?
          result.push_back(std::make_pair(entity, Lua::LuaTypeInfo<ComponentT>::fromTop(state.get())));
        }
        lua_pop(state.get(), 1);
      }
      //Pop entity and table value
      lua_pop(state.get(), 2);
    }

    return result;
  }
};