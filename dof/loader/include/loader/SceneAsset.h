#pragma once

#include "generics/Hash.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace Loader {
  struct Transform2D {
    constexpr static size_t KEY = gnx::Hash::constHash("Transform2D");
    glm::vec2 pos{};
    float rot{};
  };

  struct Transform3D {
    constexpr static size_t KEY = gnx::Hash::constHash("Transform3D");
    glm::vec3 pos{};
    float rot{};
  };

  struct Velocity3D {
    constexpr static size_t KEY = gnx::Hash::constHash("Velocity3D");
    glm::vec3 linear{};
    float angular{};
  };

  struct CollisionMask {
    constexpr static size_t KEY = gnx::Hash::constHash("CollisionMask");
    uint8_t mask{};
  };

  struct ConstraintMask {
    constexpr static size_t KEY = gnx::Hash::constHash("ConstraintMask");
    uint8_t mask{};
  };

  struct QuadUV {
    glm::vec2 min{};
    glm::vec2 max{};
  };

  struct Player {
    Transform3D transform;
    Velocity3D velocity;
    CollisionMask collisionMask;
    ConstraintMask constraintMask;
    QuadUV uv;
  };

  struct Thickness {
    constexpr static size_t KEY = gnx::Hash::constHash("Thickness");
    float thickness{};
  };

  struct Scale2D {
    constexpr static size_t KEY = gnx::Hash::constHash("Scale2D");
    glm::vec2 scale{};
  };

  struct Terrain {
    Transform3D transform;
    Velocity3D velocity;
    Scale2D scale;
    CollisionMask collisionMask;
    ConstraintMask constraintMask;
    QuadUV uv;
  };

  struct PlayerTable {
    std::vector<Player> players;
    Thickness thickness;
    MeshIndex meshIndex;
  };

  struct TerrainTable {
    std::vector<Terrain> terrains;
    Thickness thickness;
    MeshIndex meshIndex;
  };

  struct FragmentAsset {
  };

  struct SceneAsset {
    PlayerTable player;
    TerrainTable terrain;
    //TODO: ideally this would be AssetHandles pointing at other assets so they could be reused between scenes
    std::vector<MaterialAsset> materials;
    std::vector<MeshAsset> meshes;
  };

  struct SceneAssetRow : Row<SceneAsset> {};
}