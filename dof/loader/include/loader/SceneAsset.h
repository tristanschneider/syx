#pragma once

#include "AssetHandle.h"
#include "generics/Hash.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

class RuntimeDatabase;

namespace Loader {
  struct AssetHandle;

  struct Transform {
    glm::vec3 pos{};
    glm::vec3 scale{};
    float rot{};
  };

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

  struct FragmentCount {
    constexpr static size_t KEY = gnx::Hash::constHash("FragmentCount");
    size_t count{};
  };

  struct FragmentSpawner {
    Transform3D transform;
    Scale2D scale;
    FragmentCount fragmentCount;
    //The material on this mesh is what the fragments will be subsections of
    MeshIndex meshIndex;
    //Mask copied over to the fragments. Not relevant to the spawner itself which has no collision
    CollisionMask collisionMask;
  };

  struct FragmentSpawnerTable {
    constexpr static size_t KEY = gnx::Hash::constHash("FragmentSpawner");
    std::vector<FragmentSpawner> spawners;
  };

  struct SceneAsset {
    SceneAsset();
    SceneAsset(SceneAsset&&);
    ~SceneAsset();

    SceneAsset& operator=(SceneAsset&&);

    std::unique_ptr<RuntimeDatabase> db;
    PlayerTable player;
    TerrainTable terrain;
    FragmentSpawnerTable fragmentSpawners;
    //Pointing at a table with MaterialAssets
    std::vector<AssetHandle> materials;
    //Pointing at a table with MeshAssets
    std::vector<AssetHandle> meshes;
    
  };

  struct SceneAssetRow : Row<SceneAsset> {};
}