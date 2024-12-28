#pragma once

#include "loader/SceneAsset.h"
#include "QueryAlias.h"

struct RuntimeRow;

namespace Loader {
  struct LoadFailure {};

  struct AssetVariant {
    using Variant = std::variant<
      std::monostate,
      LoadFailure,
      SceneAsset
    >;

    Variant& operator*() { return v; }
    const Variant& operator*() const { return v; }

    Variant v;
  };

  struct AssetOperations {
    using StoreFN = void(*)(RuntimeRow&, AssetVariant&&, size_t);
    QueryAliasBase destinationRow{};
    StoreFN writeToDestination{};
  };

  AssetOperations getAssetOperations(const AssetVariant& v);
}