#pragma once

#include "config/Config.h"

#include <variant>

namespace ConfigIO {
  struct Result {
    struct Error {
      std::string message;
    };
    using Variant = std::variant<Config::RawGameConfig, Error>;
    Variant value;
  };

  std::string serializeJSON(const Config::RawGameConfig& config);
  Result deserializeJson(const std::string& buffer);
}