#include "Precompile.h"
#include "AssetVariant.h"

#include "RuntimeDatabase.h"
#include "generics/Functional.h"

namespace Loader {
  template<IsRow RowT, class FN>
  AssetOperations createAssetOperations() {
    struct Write {
      static void write(IRow& dst, AssetVariant&& toMove, size_t i) {
        static_cast<RowT*>(&dst)->at(i) = FN{}(std::move(toMove.v));
      }
    };

    return AssetOperations {
      .destinationRow{ QueryAlias<RowT>::create() },
      .writeToDestination{ &Write::write },
      .isFailure = false,
    };
  }

  AssetOperations failure() {
    return AssetOperations{ .isFailure = true };
  }

  struct GetAssetOperations {
    AssetOperations operator()(std::monostate) const { return failure(); }
    AssetOperations operator()(const LoadFailure&) const { return failure(); }
    AssetOperations operator()(const LoadingSceneAsset&) const {
      using namespace gnx::func;
      return createAssetOperations<SceneAssetRow, FMap<StdGet<LoadingSceneAsset>, GetMember<&LoadingSceneAsset::finalAsset>>>();
    };
    AssetOperations operator()(const MaterialAsset&) const {
      return createAssetOperations<MaterialAssetRow, gnx::func::StdGet<MaterialAsset>>();
    }
    AssetOperations operator()(const MeshAsset&) const {
      return createAssetOperations<MeshAssetRow, gnx::func::StdGet<MeshAsset>>();
    }
    AssetOperations operator()(const EmptyAsset&) const {
      return AssetOperations{ .isFailure = false };
    }
  };

  AssetOperations getAssetOperations(const AssetVariant& v) {
    return std::visit(GetAssetOperations{}, v.v);
  }
}