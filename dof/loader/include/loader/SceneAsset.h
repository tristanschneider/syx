#pragma once

#include "AssetHandle.h"
#include "generics/Hash.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "Table.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "DBTypeID.h"

class RuntimeDatabase;

namespace Loader {
  struct AssetHandle;

  struct Transform {
    glm::vec3 pos{};
    glm::vec3 scale{};
    float rot{};
  };

  struct SceneAsset {
    SceneAsset();
    SceneAsset(SceneAsset&&);
    ~SceneAsset();

    SceneAsset& operator=(SceneAsset&&);

    std::unique_ptr<RuntimeDatabase> db;
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

  //Contents of the scene are exposed in the RuntimeDatabase. The table names are parsed directly
  //The row names for generic types are one of the below types with a string key provided by the asset
  //They can be accessed with getDynamicRowKey which will succeed if the type and name match.
  //There are also hardcoded fields like TransformRow and MatMeshRefRow where the key is always Row::KEY
  using Bitfield = uint64_t;
  //Boolean in blender
  struct BoolRow : Row<uint8_t> {};
  //Boolean array in blender. Only allows up to 64 array size
  struct BitfieldRow : Row<Bitfield> {};
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