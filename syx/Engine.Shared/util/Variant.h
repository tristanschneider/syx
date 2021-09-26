#pragma once
#include <variant>

struct VariantOwned {
  virtual ~VariantOwned() = default;
};

//Alias for general purpose cases
struct Variant {
  std::variant<std::monostate, uint64_t, int, double, void*, std::shared_ptr<VariantOwned>, std::string> mData;
};
