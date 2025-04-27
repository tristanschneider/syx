#pragma once

#include "AssetHandle.h"
#include "generics/Hash.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "Table.h"
#include "DBTypeID.h"
#include "Reflection.h"

class RuntimeDatabase;
class RuntimeTable;

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

  namespace details {
    IRow* tryGetRow(RuntimeTable& table, DBTypeID id);
  }

  template<IsRow R>
  R* tryGetDynamicRow(RuntimeTable& table, std::string_view key) {
    return static_cast<R*>(details::tryGetRow(table, getDynamicRowKey<R>(key)));
  }

  template<IsLoadableRow R>
  R* tryGetDynamicRow(RuntimeTable& table) {
    return tryGetDynamicRow<R>(table, R::KEY);
  }

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