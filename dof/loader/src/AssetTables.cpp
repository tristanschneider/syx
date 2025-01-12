#include "Precompile.h"
#include "AssetTables.h"

#include "AppBuilder.h"

namespace Loader {
  KnownTables::KnownTables(RuntimeDatabaseTaskBuilder& task)
    : requests{ task.queryTables<RequestedTagRow>()[0] }
    , loading{ task.queryTables<LoadingTagRow>()[0] }
    , failed{ task.queryTables<FailedTagRow>()[0] }
    , succeeded(std::move(task.queryTables<SucceededTagRow>().getMatchingTableIDs()))
  {
  }

  LoadingAsset::~LoadingAsset() = default;
}