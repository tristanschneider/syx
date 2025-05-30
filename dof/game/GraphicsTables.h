#pragma once

#include "loader/AssetHandle.h"
#include "Table.h"

//Shared reference for all objects in the table to use
struct TextureReference {
  Loader::AssetHandle asset;
};

struct MeshReference {
  Loader::AssetHandle asset;
};

using SharedTextureRow = SharedRow<TextureReference>;
using SharedMeshRow = SharedRow<MeshReference>;
struct MeshRow : Row<MeshReference> {};