#pragma once

#include "loader/SceneAsset.h"
#include "QueryAlias.h"

class IRow;

namespace Loader {
  struct LoadFailure {};
  //Similar to a load failure but allows the load of other assets in the group to succeed
  struct EmptyAsset {};

  struct AssetVariant {
    using Variant = std::variant<
      std::monostate,
      LoadFailure,
      EmptyAsset,
      MaterialAsset,
      MeshAsset,
      SceneAsset
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