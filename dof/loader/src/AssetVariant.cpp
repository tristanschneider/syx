#include "Precompile.h"
#include "AssetVariant.h"

#include "RuntimeDatabase.h"

namespace Loader {
  template<IsRow RowT, class AssetT>
  AssetOperations createAssetOperations() {
    struct Write {
      static void write(RuntimeRow& dst, AssetVariant&& toMove, size_t i) {
        static_assert(std::is_same_v<AssetT, typename RowT::ElementT>);
        static_cast<RowT*>(dst.row)->at(i) = std::move(std::get<AssetT>(toMove.v));
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
    AssetOperations operator()(const SceneAsset&) const {
      return createAssetOperations<SceneAssetRow, SceneAsset>();
    };
    AssetOperations operator()(const MaterialAsset&) const {
      return createAssetOperations<MaterialAssetRow, MaterialAsset>();
    }
    AssetOperations operator()(const MeshAsset&) const {
      return createAssetOperations<MeshAssetRow, MeshAsset>();
    }
    AssetOperations operator()(const EmptyAsset&) const {
      return AssetOperations{ .isFailure = false };
    }
  };

  AssetOperations getAssetOperations(const AssetVariant& v) {
    return std::visit(GetAssetOperations{}, v.v);
  }
}