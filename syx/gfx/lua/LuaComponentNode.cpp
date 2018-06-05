#include "Precompile.h"
#include "lua/LuaComponentNode.h"

#include "component/Component.h"
#include "component/LuaComponentRegistry.h"
#include "lua/LuaStackAssert.h"
#include "system/LuaGameSystem.h"

namespace Lua {
  const char* ComponentNode::TYPE_KEY = "type";
  const char* ComponentNode::PROPS_KEY = "props";

  void ComponentNode::_readFromLua(lua_State* s, void* base) const {
    Lua::StackAssert sa(s);
    if(lua_getfield(s, -1, TYPE_KEY) == LUA_TSTRING) {
      const LuaGameSystem& game = LuaGameSystem::check(s);
      //Read type name and construct default from it
      std::unique_ptr<Component>& comp = _cast(base);
      comp = game.getComonentRegistry().construct(lua_tostring(s, -1), 0);

      //If type was valid, fill it in from properties table
      if(comp) {
        if(lua_getfield(s, -2, PROPS_KEY) == LUA_TTABLE) {
          if(const Node* props = comp->getLuaProps()) {
            lua_pushvalue(s, -1);
            props->readFromLua(s, comp.get(), SourceType::FromStack);
            lua_pop(s, 1);
          }
        }
        //Pop props table
        lua_pop(s, 1);
      }
    }
    //Pop name
    lua_pop(s, 1);
  }

  void ComponentNode::_writeToLua(lua_State* s, const void* base) const {
    Lua::StackAssert sa(s, 1);
    const std::unique_ptr<Component>& comp = _cast(base);

    if(comp) {
      //Create table for this component
      lua_createtable(s, 0, 2);
      //Put type in table
      lua_pushstring(s, TYPE_KEY);
      lua_pushstring(s, comp->getTypeInfo().mTypeName.c_str());
      lua_settable(s, -3);

      //Put props in table
      lua_pushstring(s, PROPS_KEY);
      if(const Node* props = comp->getLuaProps())
        props->writeToLua(s, comp.get(), SourceType::FromStack);
      else
        lua_pushnil(s);
      lua_settable(s, -3);
    }
    else
      lua_pushnil(s);
  }

  void ComponentNode::_copyConstruct(const void* from, void* to) const {
    const std::unique_ptr<Component>& src = _cast(from);
    std::unique_ptr<Component>& dest = *new (to) std::unique_ptr<Component>();
    if(src)
      dest = src->clone();
  }

  void ComponentNode::_copy(const void* from, void* to) const {
    _cast(to) = _cast(from)->clone();
  }
}