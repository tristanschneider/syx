#pragma once

#include "file/FilePath.h"

//Request to load the given asset. This entity will have the relevant asset components added to it
struct AssetLoadRequestComponent {
  FilePath mPath;
};

//Added when the asset is completely finished loading
struct AssetComponent {
};

struct AssetLoadFailedComponent {
};

struct AssetLoadingFromDiskComponent {
};

struct AssetParsingComponent {
};