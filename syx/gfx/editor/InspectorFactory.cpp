#include "Precompile.h"
#include "InspectorFactory.h"

#include "asset/Asset.h"
#include "editor/DefaultInspectors.h"
#include "editor/event/EditorEvents.h"
#include "editor/util/ScopedImGui.h"
#include <event/EventBuffer.h>
#include <imgui/imgui.h>
#include "ImGuiImpl.h"
#include "lua/LuaNode.h"
#include "lua/LuaVariant.h"
#include "provider/MessageQueueProvider.h"
#include "system/AssetRepo.h"
#include "util/Finally.h"
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

  bool inspectTransform(const Lua::Node& prop, Syx::Mat4& mat) {
    const char* name = prop.getName().c_str();
    ImGui::Text(name);
    ScopedStringId propScope(name);

    Syx::Vec3 translate, scale;
    Syx::Mat3 mRot;
    mat.decompose(scale, mRot, translate);
    Syx::Quat qRot = mRot.toQuat();
    bool changed = false;

    changed = ImGui::InputFloat3("Translate", translate.data());
    changed = ImGui::InputFloat4("Rotate", qRot.mV.data()) || changed;
    changed = ImGui::InputFloat3("Scale", scale.data()) || changed;

    if(changed) {
      //Validate parameters so that transform is still a valid affine transformation
      for(int i = 0; i < 3; ++i)
        scale[i] = std::max(0.0001f, scale[i]);
      mat = Syx::Mat4::transform(scale, qRot.safeNormalized(), translate);
      return true;
    }
    return false;
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

  bool inspectFloat(const char* name, float& data) {
    return ImGui::InputFloat(name, &data);
  }

  bool inspectFloat(const Lua::Node& prop, float& data) {
    return inspectFloat(prop.getName().c_str(), data);
  }

  bool inspectDouble(const char* name, double& data) {
    float temp = static_cast<float>(data);
    if(inspectFloat(name, temp)) {
      data = static_cast<float>(temp);
      return true;
    }
    return false;
  }

  bool inspectDouble(const Lua::Node& prop, double& data) {
    return inspectDouble(prop.getName().c_str(), data);
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
    bool valueChanged = false;
    if(ImGui::BeginPopupModal(propName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      auto endPopup = finally([]() { ImGui::EndPopup(); });
      //Button to close modal
      if(ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
        //Clear preview
        repo.getMessageQueueProvider().getMessageQueue().get().push(PreviewAssetEvent(AssetInfo(0)));
        return false;
      }
      //Scroll view for asset list
      ImGui::BeginChild("assets", ImVec2(200, 200));
      auto endAssets = finally([]() { ImGui::EndChild(); });
      std::vector<std::shared_ptr<Asset>> assets;
      repo.getAssetsByCategory(category, assets);

      //Get the previously selected item
      pad.push(propName);
      auto popPad = finally([&pad]() { pad.pop(); });
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
          repo.getMessageQueueProvider().getMessageQueue().get().push(PreviewAssetEvent(asset->getInfo()));
        }

        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          data = asset->getInfo().mId;
          valueChanged = true;
          ImGui::CloseCurrentPopup();
          break;
        }
      }
    }
    return valueChanged;
  }

  std::function<bool(const Lua::Node&, void*)> getAssetInspector(AssetRepo& repo, std::string_view category) {
    return [&repo, category](const Lua::Node& prop, void* data) {
      return inspectAsset(prop, *reinterpret_cast<size_t*>(data), repo, category);
    };
  }

  bool inspectLuaVariant(const Lua::Node& prop, Lua::Variant& data) {
    bool changed = false;
    data.forEachChild([&changed](Lua::Variant& child) {
      const size_t type = child.getTypeId();
      if(type == typeId<double>()) {
        changed = inspectDouble(child.getKey().toString().c_str(), child.get<double>()) || changed;
      }
    });
    return changed;
  }
}