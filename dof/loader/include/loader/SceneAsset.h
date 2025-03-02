#pragma once

#include "AssetHandle.h"
#include "generics/Hash.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

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
    //Pointing at a table with MaterialAssets
    std::vector<AssetHandle> materials;
    //Pointing at a table with MeshAssets
    std::vector<AssetHandle> meshes;
    
  };

  struct SceneAssetRow : Row<SceneAsset> {};

  template<IsRow T>
  constexpr DBTypeID getDynamicRowKey(size_t rowName) {
    return { gnx::Hash::combineHashes(rowName, DBTypeID::get<std::decay_t<T>>().value) };
  }

  template<IsRow T>
  constexpr DBTypeID getDynamicRowKey(std::string_view rowName) {
    return getDynamicRowKey<T>(gnx::Hash::constHash(rowName));
  }

  template<class T>
  concept HasKey = requires() {
    { T::KEY } -> std::convertible_to<std::string_view>;
  };

  template<class T>
  concept IsLoadableRow = IsRow<T> && HasKey<T>;

  template<IsLoadableRow T>
  T* tryGetDynamicRow(RuntimeTable& table) {
    return table.tryGet<T>(T::KEY);
  }

  //Contents of the scene are exposed in the RuntimeDatabase. The table names are parsed directly
  //The row names for generic types are one of the below types with a string key provided by the asset
  //They can be accessed with getDynamicRowKey which will succeed if the type and name match.
  //There are also hardcoded fields like TransformRow and MatMeshRefRow where the key is always Row::KEY

  //Boolean in blender
  struct BoolRow : Row<uint8_t> {};
  //Boolean array in blender. Only allows up to 64 array size
  struct BitfieldRow : Row<uint64_t> {};
  //Integer in blender
  struct IntRow : Row<int32_t> {};
  //Float in blender
  struct FloatRow : Row<float> {};
  //Float arrays of various sizes in blender
  struct Vec2Row : Row<glm::vec2> {};
  struct Vec3Row : Row<glm::vec3> {};
  struct Vec4Row : Row<glm::vec4> {};
  //String in blender
  struct StringRow : Row<std::string> {};

  struct SharedBoolRow : SharedRow<uint8_t> {};
  struct SharedBitfieldRow : SharedRow<uint64_t> {};
  struct SharedIntRow : SharedRow<int32_t> {};
  struct SharedFloatRow : SharedRow<float> {};
  struct SharedVec2Row : SharedRow<glm::vec2> {};
  struct SharedVec3Row : SharedRow<glm::vec3> {};
  struct SharedVec4Row : SharedRow<glm::vec4> {};
  struct SharedStringRow : SharedRow<std::string> {};

  struct TransformRow : Row<Transform> {
    static constexpr std::string_view KEY = "transform";
  };
  struct MatMeshRef {
    AssetHandle material;
    AssetHandle mesh;
  };
  struct MatMeshRefRow : Row<MatMeshRef> {
    static constexpr std::string_view KEY = "matmesh";
  };
}