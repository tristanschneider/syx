#pragma once

class Variant;

class ScratchPad {
public:
  ScratchPad(uint8_t defaultLifetime);

  void update();
  void clear();
  void push(std::string_view key);
  void pop();
  Variant* read(std::string_view key, bool refreshLifetime = true);
  void write(std::string_view key, Variant value);

private:
  size_t _getKeyHash(std::string_view key);
  size_t _getKeyHash() const;

  //Key hash to variant, remaining frames pair
  std::unordered_map<size_t, std::pair<Variant, uint8_t>> mData;
  //Stack of sizes of the previous key
  std::stack<size_t> mKeyStack;
  std::string mKey;
  uint8_t mDefaultLifetime;
};