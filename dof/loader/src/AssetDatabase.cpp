#include "Precompile.h"
#include "AssetDatabase.h"

#include "AssetTables.h"
#include "RuntimeDatabase.h"
#include "loader/SceneAsset.h"
#include "AssetIndex.h"
#include "Database.h"

namespace Loader {
  namespace db {
    using LoadingAssetTable = Table<
      StableIDRow,
      LoadingTagRow,
      UsageTrackerBlockRow,
      LoadingAssetRow
    >;
    template<class T>
    using SucceededAssetTable = Table<
      StableIDRow,
      SucceededTagRow,
      UsageTrackerBlockRow,
      T
    >;
    using LoaderDB = Database<
      Table<GlobalsRow>,
      Table<
        StableIDRow,
        RequestedTagRow,
        LoadRequestRow,
        UsageTrackerBlockRow
      >,
      Table<
        StableIDRow,
        AssetIndexRow,
        FailedTagRow,
        UsageTrackerBlockRow
      >,
      LoadingAssetTable,
      SucceededAssetTable<SceneAssetRow>,
      SucceededAssetTable<MaterialAssetRow>,
      SucceededAssetTable<MeshAssetRow>
    >;
  }

  void createAssetDatabase(RuntimeDatabaseArgs& args) {
    DBReflect::addDatabase<db::LoaderDB>(args);
  }
}