#include "Precompile.h"
#include "loader/MaterialAsset.h"

namespace std {
  size_t hash<Loader::TextureAsset>::operator()(const Loader::TextureAsset& v) const {
    size_t result = gnx::Hash::combine(v.width, v.height, v.sampleMode, v.format);
    for(uint8_t byte : v.buffer) {
      result = gnx::Hash::combine(result, byte);
    }
    return result;
  }

  size_t hash<Loader::MaterialAsset>::operator()(const Loader::MaterialAsset& v) const {
    return std::hash<Loader::TextureAsset>{}(v.texture);
  }
}