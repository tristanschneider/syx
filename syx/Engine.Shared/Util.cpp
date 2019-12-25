#include "Precompile.h"
#include "Util.h"

namespace Util {
  std::vector<std::string_view> split(std::string_view str, std::string_view delimiter) {
    std::vector<std::string_view> result;
    const auto pushSubstring = [&result, str](size_t begin, size_t end) {
      if(begin != end)
        result.emplace_back(str.substr(begin, end - begin));
    };

    size_t begin = 0;
    size_t match = str.find(delimiter);
    const size_t delimSize = delimiter.size();
    while(match != str.npos) {
      pushSubstring(begin, match);

      begin = match + delimSize;
      match = str.find(delimiter, begin);
    }

    pushSubstring(begin, str.size());
    return result;
  }
}