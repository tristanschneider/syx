#pragma once

#include "glm/vec2.hpp"

namespace Loader {
  struct MeshAsset {
    auto operator<=>(const MeshAsset&) const = default;
    //Index into the material in the containing scene
    size_t materialIndex{};
    std::vector<glm::vec2> vertices;
    std::vector<glm::vec2> textureCoordinates;
  };

  //Index into the meshes of the scene
  struct MeshIndex {
    auto operator<=>(const MeshIndex&) const = default;

    constexpr bool isSet() const {
      return *this != MeshIndex{};
    }

    uint32_t index = std::numeric_limits<uint32_t>::max();
  };
}