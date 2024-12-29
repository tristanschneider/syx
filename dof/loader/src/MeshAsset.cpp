#include "Precompile.h"
#include "loader/MeshAsset.h"

#include <generics/Hash.h>

namespace std {
  constexpr uint32_t hashRound(float f) {
    return static_cast<uint32_t>(f*1000.f)/static_cast<uint32_t>(1000);
  }

  size_t hashContainer(const std::vector<Loader::MeshVertex>& v) {
    size_t result{};
    for(const auto& e : v) {
      result = gnx::Hash::combine(result, hashRound(e.pos.x), hashRound(e.pos.y), hashRound(e.uv.x), hashRound(e.uv.y));
    }
    return result;
  }

  size_t hash<Loader::MeshAsset>::operator()(const Loader::MeshAsset& v) const {
    return hashContainer(v.verts);
  }
};