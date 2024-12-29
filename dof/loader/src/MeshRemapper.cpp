#include "Precompile.h"
#include "MeshRemapper.h"

#include "loader/MaterialAsset.h"
#include "loader/MeshAsset.h"

namespace Loader::MeshRemapper {
  template<class T>
  concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } -> std::same_as<size_t>;
    { t == t } -> std::same_as<bool>;
  };

  template<class T>
  bool equalOrNull(const T* l, const T* r) {
    if(!l || !r) {
      return l == r;
    }
    return *l == *r;
  }

  template<Hashable H>
  size_t pointerHash(const H* h, std::hash<H>& hash) {
    return h ? hash(*h) : 0;
  }

  //Given elements and their corresponding hashes, find an index into the elements vector that matches
  template<Hashable H>
  uint32_t findMatchingElement(const H& element, size_t hash, const std::vector<H>& elements, const std::vector<size_t>& hashes) {
    auto start = hashes.begin();
    while(true) {
      //Found a hash map. This should match unless there's a hash collision
      auto found = std::find(start, hashes.end(), hash);
      if(found == hashes.end()) {
        break;
      }

      //Hash index matches element index
      const uint32_t index = static_cast<uint32_t>(std::distance(hashes.begin(), found));
      assert(static_cast<uint32_t>(elements.size()) > index);

      //Rule out hash collision. If it matches, this is the desired index
      if(elements[index] == element) {
        return index;
      }

      //Hash collision, search the rest of the container for another match
      start = found + 1;
    }

    //All elements exhausted, no match
    return std::numeric_limits<uint32_t>::max();
  }

  template<Hashable H>
  uint32_t findMatchingElement(const RemapRef<H>& element, size_t hash, const std::vector<RemapRef<H>>& elements, const std::vector<size_t>& hashes) {
    auto start = hashes.begin();
    while(true) {
      //Found a hash map. This should match unless there's a hash collision
      auto found = std::find(start, hashes.end(), hash);
      if(found == hashes.end()) {
        break;
      }

      //Hash index matches element index
      const uint32_t index = static_cast<uint32_t>(std::distance(hashes.begin(), found));
      assert(static_cast<uint32_t>(elements.size()) > index);

      //Rule out hash collision. If it matches, this is the desired index
      if(equalOrNull(elements[index].value, element.value)) {
        return index;
      }

      //Hash collision, search the rest of the container for another match
      start = found + 1;
    }

    //All elements exhausted, no match
    return std::numeric_limits<uint32_t>::max();
  }

  //Remove duplicate elements and populate remappings with a map of original index to deduplicated index
  template<Hashable H>
  void deduplicate(std::vector<H>& elements, std::vector<uint32_t>& remappings, std::vector<size_t>& hashBuffer) {
    //Put result in new container and assign at the end for convenience
    std::vector<H> result;
    result.reserve(elements.size());
    hashBuffer.reserve(elements.size());
    hashBuffer.clear();

    std::hash<H> hash;
    for(uint32_t i = 0; i < static_cast<uint32_t>(elements.size()); ++i) {
      H& element = elements[i];
      const size_t h = hash(elements[i]);
      if(uint32_t found = findMatchingElement(element, h, result, hashBuffer); found < result.size()) {
        //Match found, map to the duplicate index
        remappings[i] = found;
      }
      else {
        //No match found, add the new unique element and its hash for future matches
        remappings[i] = result.size();
        result.emplace_back(std::move(element));
        hashBuffer.emplace_back(h);
      }
    }

    elements = std::move(result);
    //Now it is deduplicated and nothing should be added to it, may as well cut off the unused memory
    elements.shrink_to_fit();
  }

  template<Hashable H>
  void deduplicate(std::vector<RemapRef<H>>& elements, std::vector<uint32_t>& remappings, std::vector<size_t>& hashBuffer) {
    //Put result in new container and assign at the end for convenience
    std::vector<RemapRef<H>> result;
    result.reserve(elements.size());
    hashBuffer.reserve(elements.size());
    hashBuffer.clear();

    std::hash<H> hash;
    for(uint32_t i = 0; i < static_cast<uint32_t>(elements.size()); ++i) {
      RemapRef<H>& element = elements[i];
      const size_t h = pointerHash(element.value, hash);
      if(uint32_t found = findMatchingElement(element, h, result, hashBuffer); found < result.size()) {
        //Match found, map to the duplicate index
        remappings[i] = found;
      }
      else {
        //No match found, add the new unique element and its hash for future matches
        remappings[i] = result.size();
        result.emplace_back(element);
        hashBuffer.emplace_back(h);
      }
    }

    elements = std::move(result);
    //Now it is deduplicated and nothing should be added to it, may as well cut off the unused memory
    elements.shrink_to_fit();
  }

  struct Remapping : IRemapping {
    Remapping(std::vector<MeshIndex>&& m)
      : map{ std::move(m) } {}

    MeshIndex remap(uint32_t meshIndex) const final {
      assert(map.size() > static_cast<size_t>(meshIndex));
      return map[meshIndex];
    }

    std::vector<MeshIndex> map;
  };

  std::unique_ptr<IRemapping> createRemapping(
    //Vertices uvs, and material indices are assumed to all be parsed together,
    //while the materialIndices are separate as referred to by materialIndices
    std::vector<MeshVerticesAsset>& vertices,
    std::vector<MeshUVsAsset>& uvs,
    const std::vector<uint32_t>& materialIndices,
    std::vector<RemapRef<MaterialAsset>>& materials
  ) {
    const size_t fullSize = vertices.size();
    assert(vertices.size() <= fullSize && uvs.size() <= fullSize && materialIndices.size() <= fullSize);
    std::vector<uint32_t> remappedVertices(fullSize);
    std::vector<uint32_t> remappedUvs(fullSize);
    std::vector<uint32_t> remappedMaterials(materials.size());
    std::vector<size_t> tempHash;

    deduplicate(vertices, remappedVertices, tempHash);
    deduplicate(uvs, remappedUvs, tempHash);
    deduplicate(materials, remappedMaterials, tempHash);

    std::vector<MeshIndex> finalMapping(fullSize);
    for(size_t i = 0; i < fullSize; ++i) {
      finalMapping[i] = MeshIndex{
        .verticesIndex = remappedVertices[i],
        .uvsIndex = remappedUvs[i],
        .materialIndex = remappedMaterials[materialIndices[i]]
      };
    }

    return std::make_unique<Remapping>(std::move(finalMapping));
  }
}