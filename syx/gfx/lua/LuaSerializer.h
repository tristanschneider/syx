#pragma once

struct lua_State;

namespace Lua {
  class Serializer {
  public:
    //numPrecision is number of decimal places numbers are rounded to, and determines the epsilon under which a number is an int
    Serializer(const std::string tab, const std::string& newline, int numPrecision);

    //Serialize the given global value
    void serializeGlobal(lua_State* s, const std::string& global, std::string& buffer);
    //Serializes the value on top of the stack and pops it
    void serializeTop(lua_State* s, std::string& buffer);

  private:
    void _serialize();
    void _serializeValue();
    bool _isSupportedKeyType(int type);
    void _newline(const char* pre);
    //Push comma seperator to string with newline, or just newline if this is first value
    void _csv(bool& first);
    //Push str wrapped in quotes
    void _quoted(const std::string& str);
    //Round to our given precision
    double _round(double d);
    double _roundInt(double d);

    std::string mTab;
    std::string mNewline;
    lua_State* mS;
    std::string* mBuffer;
    int mDepth;
    //10^numPrecision
    int mRoundDigits;
    //Epsilon under which number is considered int
    double mEpsilon;
  };
}