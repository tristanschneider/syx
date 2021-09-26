#pragma once

class AssetRepo;
class MessageQueueProvider;

namespace Lua {
  class Node;
  class Variant;
}

namespace Inspector {
  // Inspect callbacks can expect the MessageQueueProvider* to be on the scratchpad with this key
  extern const std::string_view MSG_KEY;

  template<class T>
  std::function<bool(const char*, void*)> wrap(bool (*inspector)(const char*, T&)) {
    return [inspector](const char* prop, void* data) {
      return (*inspector)(prop, *reinterpret_cast<T*>(data));
    };
  }
  bool inspectString(const char* prop, std::string& str);
  bool inspectBool(const char* prop, bool& b);
  bool inspectVec3(const char* prop, Syx::Vec3& vec);
  bool inspectMat4(const char* prop, Syx::Mat4& mat);
  bool inspectTransform(const char* prop, Syx::Mat4& mat);
  bool inspectInt(const char* prop, int& i);
  bool inspectSizeT(const char* prop, size_t& data);
  bool inspectFloat(const char* prop, float& data);
  bool inspectDouble(const char* prop, double& data);
  bool inspectAsset(const char* prop, size_t& data, std::string_view category);
  std::function<bool(const char*, void*)> getAssetInspector(std::string_view category);
  bool inspectLuaVariant(const char* prop, Lua::Variant& data);
}