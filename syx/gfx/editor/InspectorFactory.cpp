#include "Precompile.h"
#include "InspectorFactory.h"

#include <imgui/imgui.h>
#include "lua/LuaNode.h"

namespace Inspector {
  bool inspectString(const Lua::Node& prop, std::string& str) {
    const size_t textLimit = 100;
    str.reserve(textLimit);
    if(ImGui::InputText(prop.getName().c_str(), str.data(), textLimit)) {
      //Since we manually modified the internals of string, manually update size
      str.resize(std::strlen(str.data()));
      return true;
    }
    return false;
  }

  bool inspectBool(const Lua::Node& prop, bool& b) {
    return ImGui::Checkbox(prop.getName().c_str(), &b);
  }

  bool inspectVec3(const Lua::Node& prop, Syx::Vec3& vec) {
    return ImGui::InputFloat3(prop.getName().c_str(), &vec.x);
  }

  bool inspectMat4(const Lua::Node& prop, Syx::Mat4& mat) {
    const char* name = prop.getName().c_str();
    ImGui::Text(name);

    ImGui::PushID(name);
    bool changed = false;
    const char* names[] = { "##c0", "##c1", "##c2", "##c3" };
    Syx::Mat4 m = mat.transposed();
    for(int i = 0; i < 4; ++i) {
      changed = ImGui::InputFloat4(names[i], m.mColRow[i]) || changed;
    }
    if(changed)
      mat = m.transposed();
    ImGui::PopID();
    return changed;
  }

  bool inspectInt(const Lua::Node& prop, int& i) {
    return ImGui::InputInt(prop.getName().c_str(), &i);
  }

  bool inspectSizeT(const Lua::Node& prop, size_t& data) {
    int i = static_cast<int>(data);
    bool result = inspectInt(prop, i);
    data = static_cast<size_t>(std::max(0, i));
    return result;
  }

  bool inspectFloat(const Lua::Node& prop, float& data) {
    return ImGui::InputFloat(prop.getName().c_str(), &data);
  }
}