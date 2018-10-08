#pragma once

class AssetRepo;

namespace Lua {
  class Node;
}

namespace Inspector {
  bool inspectString(const Lua::Node& prop, std::string& str);
  bool inspectBool(const Lua::Node& prop, bool& b);
  bool inspectVec3(const Lua::Node& prop, Syx::Vec3& vec);
  bool inspectMat4(const Lua::Node& prop, Syx::Mat4& mat);
  bool inspectInt(const Lua::Node& prop, int& i);
  bool inspectSizeT(const Lua::Node& prop, size_t& data);
  bool inspectFloat(const Lua::Node& prop, float& data);
  bool inspectAsset(const Lua::Node& prop, size_t& data, AssetRepo& repo, std::string_view category);
  std::function<bool(const Lua::Node&, void*)> getAssetInspector(AssetRepo& repo, std::string_view category);
}