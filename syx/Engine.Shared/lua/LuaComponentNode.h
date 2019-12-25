#pragma once
//Nodes used to specify a structure to bind data to, allowing it to be
//read and written to lua. All nodes take a reference to the type to bind,
//which will be written or read. Overload makeNode for new node types.

#include <lua.hpp>
#include "lua/LuaNode.h"

struct lua_State;
class Component;

namespace Lua {
  class Node;

  class ComponentNode : public TypedNode<std::unique_ptr<Component>> {
  public:
    using TypedNode::TypedNode;

    void _readFromLua(lua_State* s, void* base) const override;
    void _writeToLua(lua_State* s, const void* base) const override;
    void _copyConstruct(const void* from, void* to) const override;
    void _copy(const void* from, void* to) const override;

  protected:
    static const char* TYPE_KEY;
    static const char* PROPS_KEY;
  };
}
