#pragma once

#include "DBTypeID.h"
#include <StableElementID.h>
#include "Table.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

namespace Loader {
  struct ObjID {
    using RawViewHash = std::hash<std::string_view>;
    auto operator<=>(const ObjID&) const = default;
    explicit operator bool() const { return value != 0; }
    size_t value{};
  };
}
namespace std {
  template<>
  struct hash<Loader::ObjID> {
    size_t operator()(const Loader::ObjID& v) const {
      return std::hash<size_t>{}(v.value);
    }
  };
}

namespace Loader {
  //An ElementRef intended for storage in scenes.
  //Its loading is a two step process where first an ObjID is loaded and then the
  //stable mappings are filled in.
  //The object uses the same amount of space as an ElementRef by using its padding space.
  class PersistentElementRef {
  public:
    ElementRef tryGetRef() const {
      return isSet ? ElementRef{ ref, expectedVersion } : ElementRef{};
    }

    ObjID tryGetID() const {
      return isSet ? ObjID{} : ObjID{ *reinterpret_cast<const size_t*>(&ref) };
    }

    void set(const ElementRef& e) {
      isSet = 1;
      ref = e.getMapping();
      expectedVersion = e.getExpectedVersion();
    }

    void set(const ObjID& id) {
      isSet = 0;
      *reinterpret_cast<size_t*>(&ref) = id.value;
    }

  private:
    StableElementMappingPtr ref;
    StableElementVersion expectedVersion{};
    uint8_t isSet{};
  };
  //Use this as the destination for an IDRefRow
  struct PersistentElementRefRow : Row<PersistentElementRef> {};

  struct ObjIDMappings {
    std::unordered_map<ObjID, ElementRef> mappings;
  };

  //Contents of the scene are exposed in the RuntimeDatabase. The table names are parsed directly
  //The row names for generic types are one of the below types with a string key provided by the asset
  //They can be accessed with getDynamicRowKey which will succeed if the type and name match.
  //There are also hardcoded fields like TransformRow and MatMeshRefRow where the key is always Row::KEY
  using Bitfield = uint64_t;
  //Boolean in blender
  struct BoolRow : Row<uint8_t> {};
  //Boolean array in blender. Only allows up to 64 array size
  struct BitfieldRow : Row<Bitfield> {};
  //Integer in blender
  struct IntRow : Row<int32_t> {};
  //Float in blender
  struct FloatRow : Row<float> {};
  //Float arrays of various sizes in blender
  struct Vec2Row : Row<glm::vec2> {};
  struct Vec3Row : Row<glm::vec3> {};
  struct Vec4Row : Row<glm::vec4> {};
  //String in blender
  struct StringRow : Row<std::string> {};
  //Hash of name of object in blender, which is always unique
  struct IDRow : Row<ObjID> {
    static constexpr std::string_view KEY = "object_id";
  };
  //Data block of ID type "object" in blender, which is then a hash of the name matching the IDRow
  struct IDRefRow : IDRow {};

  struct SharedBoolRow : SharedRow<uint8_t> {};
  struct SharedBitfieldRow : SharedRow<uint64_t> {};
  struct SharedIntRow : SharedRow<int32_t> {};
  struct SharedFloatRow : SharedRow<float> {};
  struct SharedVec2Row : SharedRow<glm::vec2> {};
  struct SharedVec3Row : SharedRow<glm::vec3> {};
  struct SharedVec4Row : SharedRow<glm::vec4> {};
  struct SharedStringRow : SharedRow<std::string> {};
  struct SharedIDRefRow : SharedRow<ObjID> {};
  //This is a singleton containing the mappings for a scene while its initialization events are in-flight
  //The expectation is that only one scene is being instantiated at a time.
  struct SharedObjIDMappingsRow : SharedRow<ObjIDMappings> {};

  template<class T>
  concept HasKey = requires() {
    { T::KEY } -> std::convertible_to<std::string_view>;
  };

  template<class T>
  concept IsLoadableRow = IsRow<T> && HasKey<T>;

  template<IsRow T>
  constexpr DBTypeID getDynamicRowKey(size_t rowName) {
    return { gnx::Hash::combineHashes(rowName, DBTypeID::get<std::decay_t<T>>().value) };
  }

  template<IsRow T>
  constexpr DBTypeID getDynamicRowKey(std::string_view rowName) {
    return getDynamicRowKey<T>(gnx::Hash::constHash(rowName));
  }

  template<IsLoadableRow T>
  constexpr DBTypeID getDynamicRowKey() {
    return getDynamicRowKey<T>(T::KEY);
  }
}