#pragma once

#include "AssetHandle.h"
#include "generics/Hash.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace Loader {
  struct AssetHandle;

  //Matches blender transform discarding Z axis
  struct Transform2D {
    constexpr static size_t KEY = gnx::Hash::constHash("Transform2D");
    glm::vec2 pos{};
    float rot{};
  };

  //Matches blender transform
  struct Transform3D {
    constexpr static size_t KEY = gnx::Hash::constHash("Transform3D");
    glm::vec3 pos{};
    float rot{};
  };

  //Populated by using custom property KEY of type float array 4:
  //Linear X Y Z and angular
  struct Velocity3D {
    constexpr static size_t KEY = gnx::Hash::constHash("Velocity3D");
    glm::vec3 linear{};
    float angular{};
  };

  //Populated by using custom property KEY of type boolean array 8
  struct CollisionMask {
    constexpr static size_t KEY = gnx::Hash::constHash("CollisionMask");

    bool isSet() const { return mask != 0; }

    uint8_t mask{};
  };

  //Populated by using custom property KEY of type boolean array 8
  struct ConstraintMask {
    constexpr static size_t KEY = gnx::Hash::constHash("ConstraintMask");

    bool isSet() const { return mask != 0; }

    uint8_t mask{};
  };

  //TODO:
  struct QuadUV {
    glm::vec2 min{};
    glm::vec2 max{};
  };

  //Elements of PlayerTable
  struct Player {
    Transform3D transform;
    Velocity3D velocity;
    CollisionMask collisionMask;
    ConstraintMask constraintMask;
    //TODO:
    QuadUV uv;
  };

  //Custom float property KEY on player table
  struct Thickness {
    constexpr static size_t KEY = gnx::Hash::constHash("Thickness");

    bool isSet() const { return thickness != 0; }

    float thickness{};
  };

  //Matches transform of blender with Z discarded
  struct Scale2D {
    constexpr static size_t KEY = gnx::Hash::constHash("Scale2D");
    glm::vec2 scale{};
  };

  //Elements of TerrainTable
  struct Terrain {
    Transform3D transform;
    Scale2D scale;
    CollisionMask collisionMask;
    ConstraintMask constraintMask;
    QuadUV uv;
  };

  //All children of a node with the given custom property "Table" matching KEY create elements in that table
  struct PlayerTable {
    constexpr static size_t KEY = gnx::Hash::constHash("Player");
    //Populated from all children of a table node
    std::vector<Player> players;
    Thickness thickness;
    //Parsed from the first mesh of one of the children as players assume using the same mesh
    MeshIndex meshIndex;
  };

  struct TerrainTable {
    constexpr static size_t KEY = gnx::Hash::constHash("Terrain");
    std::vector<Terrain> terrains;
    Thickness thickness;
    MeshIndex meshIndex;
  };

  struct FragmentAsset {
  };

  struct SceneAsset {
    ~SceneAsset();

    PlayerTable player;
    TerrainTable terrain;
    //TODO: ideally this would be AssetHandles pointing at other assets so they could be reused between scenes
    //Pointing at a table with MaterialAssets
    std::vector<AssetHandle> materials;
    std::vector<MeshVerticesAsset> meshVertices;
    std::vector<MeshUVsAsset> meshUVs;
  };

  struct SceneAssetRow : Row<SceneAsset> {};
}