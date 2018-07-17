#pragma once
#include "lua/LuaKey.h"

struct lua_State;

namespace Lua {
  class Node;

  class Variant {
  public:
    Variant();
    Variant(const Key& key);
    Variant(const Variant& rhs);
    Variant(Variant&& rhs);
    ~Variant();

    Variant& operator=(const Variant& rhs);
    Variant& operator=(Variant&& rhs);

    // Clear this and all children and populate from value on top of the stack
    bool readFromLua(lua_State* l);
    // Write this and all children to the top of the stack
    void writeToLua(lua_State* l) const;
    void clear();
    size_t getTypeId() const;
    const Variant* getChild(const Key& key) const;
    Variant* getChild(const Key& key);

    template<typename T>
    T* get() {
      return getTypeId() == typeId<T>() && mData.size() ? reinterpret_cast<T*>(mData.data()) : nullptr;
    }
    template<typename T>
    const T* get() const {
      return getTypeId() == typeId<T>() && mData.size() ? reinterpret_cast<const T*>(mData.data()) : nullptr;
    }

  private:
    void _destructData();
    void _copyData(const std::vector<uint8_t>& from);
    void _moveData(std::vector<uint8_t>& from);

    Key mKey;
    const Node* mType;
    std::vector<uint8_t> mData;
    std::vector<Variant> mChildren;
  };
}