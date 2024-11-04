#pragma once

#include "glm/vec2.hpp"

namespace Loader {
  struct MeshAsset {
    //Index into the material in the containing scene
    size_t materialIndex{};
    std::vector<glm::vec2> vertices;
    std::vector<glm::vec2> textureCoordinates;
  };
}