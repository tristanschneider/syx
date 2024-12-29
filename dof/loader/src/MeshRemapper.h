#pragma once

#include "loader/AssetHandle.h"

namespace Loader {
  struct AssetHandle;
  struct MeshIndex;
  struct MeshAsset;
  struct MaterialAsset;

  //Deduplicates redundant uv, vertex, and material information then provides a mapping
  //Of the original index to the new indices
  //TODO: might be unnecessary. Original intent was to allow same mesh with different uvs
  //This is instead accomplished with uv offsets
  namespace MeshRemapper {
    class IRemapping {
    public:
      virtual ~IRemapping() = default;
      virtual MeshIndex remap(uint32_t meshIndex) const = 0;
    };

    template<class T>
    struct RemapRef {
      const AssetHandle* handle{};
      T* value{};
    };

    struct RemapRefUnwrapper {
      template<class T>
      AssetHandle operator()(const RemapRef<T>& r) const {
        return r.handle ? *r.handle : AssetHandle{};
      }
    };

    //Removes duplicates in the source containers and provides a map of the original to new indices
    std::unique_ptr<IRemapping> createRemapping(
      std::vector<RemapRef<MeshAsset>>& meshes,
      const std::vector<uint32_t>& materialIndices,
      std::vector<RemapRef<MaterialAsset>>& materials
    );
  }
}