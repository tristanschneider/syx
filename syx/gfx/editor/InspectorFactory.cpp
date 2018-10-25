#include "Precompile.h"
#include "InspectorFactory.h"

#include "asset/Asset.h"
#include <imgui/imgui.h>
#include "ImGuiImpl.h"
#include "lua/LuaNode.h"
#include "system/AssetRepo.h"
#include "util/ScratchPad.h"
#include "util/Variant.h"

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

  bool inspectAsset(const Lua::Node& prop, size_t& data, AssetRepo& repo, std::string_view category) {
    ScratchPad& pad = ImGuiImpl::getPad();
    //Left side label for property
    const char* propName = prop.getName().c_str();
    ImGui::Text(propName);

    //Button with name of current asset
    const char* valueName = "none";
    if(std::shared_ptr<Asset> asset = repo.getAsset(AssetInfo(data))) {
      valueName = asset->getInfo().mUri.c_str();
    }
    ImGui::SameLine();
    //Open selection modal on button press
    if(ImGui::Button(valueName)) {
      ImGui::OpenPopup(propName);
    }
    ImGui::NewLine();

    //Populate asset list in modal
    if(ImGui::BeginPopupModal(propName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::BeginChild("assets", ImVec2(200, 200));
      std::vector<std::shared_ptr<Asset>> assets;
      repo.getAssetsByCategory(category, assets);

      //Get the previously selected item
      pad.push(propName);
      std::string_view selectedKey = "selectedAsset";
      Variant* selected = pad.read(selectedKey);
      size_t selectedId = selected ? selected->get<size_t>() : 0;

      for(const auto& asset : assets) {
        bool thisSelected = asset->getInfo().mId == selectedId;
        bool wasSelected = thisSelected;
        ImGui::Selectable(asset->getInfo().mUri.c_str(), &thisSelected);
        //If selection changed this frame, write the new selected item to the pad
        if(thisSelected != wasSelected) {
          size_t newSelection = thisSelected ? asset->getInfo().mId : 0;
          pad.write(selectedKey, newSelection);
          //TODO: broadcast selection message for preview
        }

        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          data = asset->getInfo().mId;
          ImGui::CloseCurrentPopup();
          break;
        }
      }

      pad.pop();
      ImGui::EndChild();
      ImGui::EndPopup();
    }
    return false;
  }

  std::function<bool(const Lua::Node&, void*)> getAssetInspector(AssetRepo& repo, std::string_view category) {
    return [&repo, category](const Lua::Node& prop, void* data) {
      return inspectAsset(prop, *reinterpret_cast<size_t*>(data), repo, category);
    };
  }
}