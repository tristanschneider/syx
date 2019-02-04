#pragma once

class AssetRepo;

namespace Lua {
  class Node;
  class Variant;
}

namespace Inspector {
  template<class T>
  std::function<bool(const Lua::Node& prop, void*)> wrap(bool (*inspector)(const Lua::Node&, T&)) {
    return [inspector](const Lua::Node& prop, void* data) {
      return (*inspector)(prop, *reinterpret_cast<T*>(data));
    };
  }
  bool inspectString(const Lua::Node& prop, std::string& str);
  bool inspectBool(const Lua::Node& prop, bool& b);
  bool inspectVec3(const Lua::Node& prop, Syx::Vec3& vec);
  bool inspectMat4(const Lua::Node& prop, Syx::Mat4& mat);
  bool inspectTransform(const Lua::Node& prop, Syx::Mat4& mat);
  bool inspectInt(const Lua::Node& prop, int& i);
  bool inspectSizeT(const Lua::Node& prop, size_t& data);
  bool inspectFloat(const Lua::Node& prop, float& data);
  bool inspectDouble(const Lua::Node& prop, double& data);
  bool inspectAsset(const Lua::Node& prop, size_t& data, AssetRepo& repo, std::string_view category);
  std::function<bool(const Lua::Node&, void*)> getAssetInspector(AssetRepo& repo, std::string_view category);
  bool inspectLuaVariant(const Lua::Node& prop, Lua::Variant& data);
}