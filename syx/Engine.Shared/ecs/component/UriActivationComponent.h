#pragma once

#include "Util.h"

struct UriActivationComponent {
  static std::unordered_map<std::string, std::string> parseUri(const std::string& uri) {
    const std::vector<std::string_view> params = Util::split(uri, " ");
    std::unordered_map<std::string, std::string> result;
    for(std::string_view param : params) {
      const std::vector<std::string_view> keyValue = Util::split(param, "=");
      if(keyValue.size() == 2) {
        result[std::string(keyValue[0])] = std::string(keyValue[1]);
      }
      else {
        printf("Invalid argument %s\n", param.data());
      }
    }
    return result;
  }

  std::string mUri;
};