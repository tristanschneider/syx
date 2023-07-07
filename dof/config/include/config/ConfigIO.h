#pragma once

#include "config/Config.h"

#include <variant>

namespace ConfigIO {
  struct Result {
    struct Error {
      std::string message;
    };
    using Variant = std::variant<Config::GameConfig, Error>;
    Variant value;
  };

  std::string serializeJSON(const Config::GameConfig& config);
  Result deserializeJson(const std::string& buffer, const Config::IFactory& factory);
  //Provide defined values for config for when deserialization fails
  void defaultInitialize(const Config::IFactory& factory, Config::GameConfig& result);
}