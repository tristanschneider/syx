#pragma once

#include "loader/SceneAsset.h"
#include "QueryAlias.h"
#include "RuntimeDatabase.h"
#include <variant>

class IRow;

namespace Loader {
  struct LoadFailure {};
  //Similar to a load failure but allows the load of other assets in the group to succeed
  struct EmptyAsset {};

  struct LoadingSceneAsset {
    SceneAsset finalAsset;
    RuntimeDatabaseArgs loadingArgs;
  };

  struct AssetVariant {
    using Variant = std::variant<
      std::monostate,
      LoadFailure,
      EmptyAsset,
      MaterialAsset,
      MeshAsset,
      LoadingSceneAsset
    >;

    Variant& operator*() { return v; }
    const Variant& operator*() const { return v; }
    Variant* operator->() { return &v; }
    const Variant* operator->() const { return &v; }

    Variant v;
  };

  struct AssetOperations {
    using StoreFN = void(*)(IRow&, AssetVariant&&, size_t);
    QueryAliasBase destinationRow{};
    StoreFN writeToDestination{};
    bool isFailure{};
  };

  AssetOperations getAssetOperations(const AssetVariant& v);
}