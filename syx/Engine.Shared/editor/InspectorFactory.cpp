#include "Precompile.h"
#include "InspectorFactory.h"

#include "asset/Asset.h"
#include "asset/ImmediateAssetWrapper.h"
#include "editor/DefaultInspectors.h"
#include "editor/Editor.h"
#include "editor/event/EditorEvents.h"
#include "editor/util/ScopedImGui.h"
#include "event/AssetEvents.h"
#include <event/EventBuffer.h>
#include <imgui/imgui.h>
#include "ImGuiImpl.h"
#include "lua/LuaNode.h"
#include "lua/LuaVariant.h"
#include "provider/MessageQueueProvider.h"
#include "util/Finally.h"
#include "util/ScratchPad.h"
#include "util/Variant.h"

namespace Inspector {
  const std::string_view MSG_KEY = "msg";

  bool inspectString(const char* prop, std::string& str) {
    const size_t textLimit = 100;
    str.reserve(textLimit);
    if(ImGui::InputText(prop, str.data(), textLimit)) {
      //Since we manually modified the internals of string, manually update size
      str.resize(std::strlen(str.data()));
      return true;
    }
    return false;
  }

  bool inspectBool(const char* prop, bool& b) {
    return ImGui::Checkbox(prop, &b);
  }

  bool inspectVec3(const char* prop, Syx::Vec3& vec) {
    return ImGui::InputFloat3(prop, &vec.x);
  }

  bool inspectMat4(const char* prop, Syx::Mat4& mat) {
    ImGui::Text(prop);

    ImGui::PushID(prop);
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

  bool inspectTransform(const char* prop, Syx::Mat4& mat) {
    ImGui::Text(prop);
    ScopedStringId propScope(prop);

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

  bool inspectInt(const char* prop, int& i) {
    return ImGui::InputInt(prop, &i);
  }

  bool inspectSizeT(const char* prop, size_t& data) {
    int i = static_cast<int>(data);
    bool result = inspectInt(prop, i);
    data = static_cast<size_t>(std::max(0, i));
    return result;
  }

  bool inspectFloat(const char* name, float& data) {
    return ImGui::InputFloat(name, &data);
  }

  bool inspectDouble(const char* name, double& data) {
    float temp = static_cast<float>(data);
    if(inspectFloat(name, temp)) {
      data = static_cast<float>(temp);
      return true;
    }
    return false;
  }

  struct InspectAssetContext : public VariantOwned {
    struct CategoryQuery {
      std::string mCategory;
      std::vector<std::shared_ptr<Asset>> mResults;
    };

    std::shared_ptr<Asset> mCurrentAsset;
    CategoryQuery mCategoryQuery;
  };

  bool inspectAsset(const char* prop, size_t& data, std::string_view category) {
    ScratchPad& pad = IImGuiImpl::getPad();

    //The caller is responsible for settings this
    MessageQueueProvider* msg = static_cast<MessageQueueProvider*>(std::get<void*>(pad.read(MSG_KEY)->mData));

    //Left side label for property
    ImGui::Text(prop);

    //Get the previously selected item
    pad.push(prop);
    auto popPad = finally([&pad]() { pad.pop(); });
    const std::string_view contextKey = "context";
    Variant* contextVariant = pad.read(contextKey);
    std::shared_ptr<InspectAssetContext> context;
    if(contextVariant) {
      context = std::static_pointer_cast<InspectAssetContext>(std::get<std::shared_ptr<VariantOwned>>(contextVariant->mData));
    }
    else {
      context = std::make_shared<InspectAssetContext>();
      pad.write(contextKey, Variant{ std::static_pointer_cast<VariantOwned>(context) });
    }

    //Button with name of current asset
    const char* valueName = "none";
    if (!context->mCurrentAsset || context->mCurrentAsset->getInfo().mId != data) {
      context->mCurrentAsset = ImmediateAsset::create(AssetInfo(data), msg->getMessageQueue(), typeId<Editor, System>());
    }
    if(!context->mCurrentAsset->getInfo().mUri.empty()) {
      valueName = context->mCurrentAsset->getInfo().mUri.c_str();
    }
    ImGui::SameLine();
    //Open selection modal on button press
    if(ImGui::Button(valueName)) {
      ImGui::OpenPopup(prop);
    }
    ImGui::NewLine();

    //Populate asset list in modal
    bool valueChanged = false;
    if(ImGui::BeginPopupModal(prop, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      auto endPopup = finally([]() { ImGui::EndPopup(); });
      //Button to close modal
      if(ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
        //Clear preview
        msg->getMessageQueue().get().push(PreviewAssetEvent(AssetInfo(0)));
        return false;
      }
      //Scroll view for asset list
      ImGui::BeginChild("assets", ImVec2(200, 200));
      auto endAssets = finally([]() { ImGui::EndChild(); });

      //Start a new query for the asset category if none exists yet
      if(context->mCategoryQuery.mCategory != category) {
        context->mCategoryQuery.mCategory = category;
        msg->getMessageQueue()->push(AssetQueryRequest(std::string(category)).then(typeId<Editor, System>(), [context, requestedCategory(std::string(category))](const AssetQueryResponse& e) {
          //Make sure not to ionvalidate a more recent request
          if(requestedCategory == context->mCategoryQuery.mCategory) {
            context->mCategoryQuery.mResults = e.mResults;
          }
        }));
      }

      std::string_view selectedKey = "selectedAsset";
      Variant* selected = pad.read(selectedKey);
      size_t selectedId = selected ? std::get<size_t>(selected->mData) : 0;

      for(const auto& asset : context->mCategoryQuery.mResults) {
        bool thisSelected = asset->getInfo().mId == selectedId;
        bool wasSelected = thisSelected;
        ImGui::Selectable(asset->getInfo().mUri.c_str(), &thisSelected);
        //If selection changed this frame, write the new selected item to the pad
        if(thisSelected != wasSelected) {
          size_t newSelection = thisSelected ? asset->getInfo().mId : 0;
          pad.write(selectedKey, Variant{ newSelection });
          msg->getMessageQueue().get().push(PreviewAssetEvent(asset->getInfo()));
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

  std::function<bool(const char*, void*)> getAssetInspector(std::string_view category) {
    return [category](const char* prop, void* data) {
      return inspectAsset(prop, *reinterpret_cast<size_t*>(data), category);
    };
  }

  bool inspectLuaVariant(const char*, Lua::Variant& data) {
    bool changed = false;
    data.forEachChild([&changed](Lua::Variant& child) {
      static DefaultInspectors inspectors;
      if(const Lua::Node* type = child.getType()) {
        if(auto inspector = inspectors.getFactory(*type)) {
          changed = (*inspector)(child.getKey().toString().c_str(), child.data()) || changed;
        }
      }
    });
    return changed;
  }
}