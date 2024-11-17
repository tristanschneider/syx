#pragma once

namespace Loader {
  struct MeshIndex;
  struct MeshVerticesAsset;
  struct MeshUVsAsset;
  struct MaterialAsset;

  //Deduplicates redundant uv, vertex, and material information then provides a mapping
  //Of the original index to the new indices
  namespace MeshRemapper {
    class IRemapping {
    public:
      virtual ~IRemapping() = default;
      virtual MeshIndex remap(uint32_t meshIndex) const = 0;
    };

    //Removes duplicates in the source containers and provides a map of the original to new indices
    std::unique_ptr<IRemapping> createRemapping(
      std::vector<MeshVerticesAsset>& vertices,
      std::vector<MeshUVsAsset>& uvs,
      const std::vector<uint32_t>& materialIndices,
      std::vector<MaterialAsset>& materials
    );
  }
}