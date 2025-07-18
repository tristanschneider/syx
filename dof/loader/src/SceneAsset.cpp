#include "Precompile.h"

#include "loader/AssetHandle.h"
#include "loader/SceneAsset.h"

#include "RuntimeDatabase.h"

namespace Loader {
  SceneAsset::~SceneAsset() = default;
  SceneAsset::SceneAsset() = default;
  SceneAsset::SceneAsset(SceneAsset&&) = default;

  SceneAsset& SceneAsset::operator=(SceneAsset&&) = default;

  IRow* details::tryGetRow(RuntimeTable& table, DBTypeID id) {
    return table.tryGet(id);
  }
}