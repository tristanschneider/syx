#include "Precompile.h"
#include "loader/MeshAsset.h"

#include <generics/Hash.h>

namespace std {
  constexpr uint32_t hashRound(float f) {
    return static_cast<uint32_t>(f*1000.f)/static_cast<uint32_t>(1000);
  }

  size_t hashContainer(const std::vector<glm::vec2>& v) {
    size_t result{};
    for(const auto& e : v) {
      result = gnx::Hash::combine(result, hashRound(e.x), hashRound(e.y));
    }
    return result;
  }

  size_t hash<Loader::MeshVerticesAsset>::operator()(const Loader::MeshVerticesAsset& v) const {
    return hashContainer(v.vertices);
  }

  size_t hash<Loader::MeshUVsAsset>::operator()(const Loader::MeshUVsAsset& v) const {
    return hashContainer(v.textureCoordinates);
  }
};