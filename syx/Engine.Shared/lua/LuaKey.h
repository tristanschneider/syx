#pragma once

struct lua_State;

namespace Lua {
  class Key {
  public:
    Key(std::string key);
    Key(const char* key);
    Key(int key = 0);

    bool operator==(const Key& key) const;
    bool operator!=(const Key& key) const;

    int push(lua_State* l) const;
    bool readFromLua(lua_State* l, int index);
    size_t getHash() const;
    std::string toString() const;

  private:
    std::string mStr;
    union {
      int mIndex;
      size_t mHash;
    };
  };
}