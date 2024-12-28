#pragma once

#include "loader/AssetLoader.h"
#include "Table.h"
#include <shared_mutex>

class ElementRef;

namespace Loader {
  struct AssetLocation;

  //TODO: Do I need this?
  class AssetIndex {
  public:
    ~AssetIndex();

    ElementRef find(const AssetLocation& key) const;
    void insert(AssetLocation&& key, const ElementRef& value);
    void erase(const AssetLocation& key);

  private:
    mutable std::shared_mutex mutex;
    std::unordered_map<AssetLocation, ElementRef> index;
  };

  struct AssetIndexRow : SharedRow<AssetIndex> {};
}