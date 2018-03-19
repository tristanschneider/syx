#pragma once

struct lua_State;

namespace Lua {
  class NumArray {
  public:
    NumArray(size_t size);

    int size() const;
    float get(int i) const;
    void set(int i, float value);
    std::string toString() const;

    static void openLib(lua_State* l);
    static int construct(lua_State* l);
    //Number get(array, index)
    static int get(lua_State* l);
    //void set(array, index, value)
    static int set(lua_State* l);
    //Number size()
    static int size(lua_State* l);
    //string __tostring(array)
    static int toString(lua_State* l);
    //number __index(array, index)
    //function __index(array, string)
    static int indexOverload(lua_State* l);
    //void __newindex(array, index, number)
    //void __newindex(array, key, value)
    static int newindexOverload(lua_State* l);

  private:
    static const char* CLASS_NAME;

    static NumArray* _getArray(lua_State* l, int i);
    static int _getIndex(lua_State* l, const NumArray& arr, int i);
    static float _getValue(lua_State* l, int i);

    int mSize;
    //Over allocated to have mSize elements
    float mData[1];
  };
}