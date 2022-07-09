#pragma once

#include "file/FilePath.h"
#include "TypeInfo.h"

//Request to load the given asset. This entity will have the relevant asset components added to it
struct AssetLoadRequestComponent {
  FilePath mPath;
};

//Added after processing the load request used to store information relevant to loaders or display
struct AssetInfoComponent {
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

namespace ecx {
  template<>
  struct StaticTypeInfo<AssetInfoComponent> : StructTypeInfo<StaticTypeInfo<AssetInfoComponent>,
    AutoTypeList<&AssetInfoComponent::mPath>,
    AutoTypeList<>> {
    static inline const std::array<std::string, 1> MemberNames = { "path" };
    static inline constexpr const char* SelfName = "AssetInfo";
  };
}