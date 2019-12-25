#pragma once

struct lua_State;

namespace Lua {
  class NumVec {
  public:
    NumVec(size_t reserve);
    ~NumVec();

    std::string toString() const;

    static void openLib(lua_State* l);
    //NumVec(reserve?)
    static int construct(lua_State* l);
    static int destruct(lua_State* l);
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
    //bool empty()
    static int empty(lua_State* l);
    //void clear()
    static int clear(lua_State* l);
    //int find(number)
    static int find(lua_State* l);
    //void resize(int)
    static int resize(lua_State* l);
    //void reserve(int)
    static int reserve(lua_State* l);
    //void pushBack(number)
    static int pushBack(lua_State* l);
    //void popVack()
    static int popBack(lua_State* l);

  private:
    static const char* CLASS_NAME;

    static NumVec* _getVec(lua_State* l, int i);
    static int _getIndex(lua_State* l, const NumVec& v, int i);
    static float _getValue(lua_State* l, int i);
    static size_t _getSize(lua_State* l, int i);

    std::vector<float> mVec;
  };
}