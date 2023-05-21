#include "Precompile.h"
#include "config/ConfigIO.h"

#include <cereal/archives/json.hpp>

namespace ConfigIO {
  //template<class Archiver>
  //void archive(

  template<class Archiver>
  void serialize(Archiver& archive, const Config::RawGameConfig& value) {
    archive;value;
  }

  std::string serializeJSON(const Config::RawGameConfig& config) {
    std::stringstream stream;
    cereal::JSONOutputArchive archive(stream);
    archive(config);
    return stream.str();
  }

  Result deserializeJson(const std::string& buffer) {
    buffer;
    return {};
  }
}
