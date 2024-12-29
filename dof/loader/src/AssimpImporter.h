#pragma once

struct AppTaskArgs;

namespace Loader {
  class IAssetImporter;
  struct AssetLoadTask;

  //Single-use reader for a given request
  std::unique_ptr<IAssetImporter> createAssimpImporter(AssetLoadTask&, const AppTaskArgs&);
}