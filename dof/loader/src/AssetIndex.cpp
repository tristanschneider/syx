#include "Precompile.h"
#include "AssetIndex.h"

#include "StableElementID.h"

namespace Loader {
  AssetIndex::~AssetIndex() = default;

  ElementRef AssetIndex::find(const AssetLocation& key) const {
    std::shared_lock lock{ mutex };
    auto it = index.find(key);
    return it != index.end() ? it->second : ElementRef{};
  }

  void AssetIndex::insert(AssetLocation&& key, const ElementRef& value) {
    std::unique_lock lock{ mutex };
    index.emplace(std::make_pair(std::move(key), value));
  }

  void AssetIndex::erase(const AssetLocation& key) {
    std::unique_lock lock{ mutex };
    index.erase(key);
  }
}