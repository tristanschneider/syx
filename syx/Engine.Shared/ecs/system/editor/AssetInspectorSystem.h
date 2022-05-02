#pragma once

#include "ecs/component/AssetComponent.h"
#include "ecs/component/EditorComponents.h"
#include "ecs/component/ImGuiContextComponent.h"
#include "ecs/system/editor/ObjectInspectorTraits.h"
#include "ecs/ECS.h"
#include "editor/Picker.h"
#include "imgui/imgui.h"
#include "TypeInfo.h"
#include "util/Finally.h"

template<class AssetT>
struct AssetInspectorSystem {
  using ImGuiView = Engine::View<Engine::Include<ImGuiContextComponent>>;
  using Modifier = Engine::EntityModifier<SelectedComponent>;
  //Filter on AssetComponent because that indicates the asset is done loading, not so for only AssetInfoComponent or the specific asset type
  using AssetView = Engine::View<Engine::Include<AssetComponent>, Engine::Include<AssetT>, Engine::Read<AssetInfoComponent>>;
  using ContextView = Engine::View<Engine::Write<InspectedAssetModalComponent>, Engine::Include<InspectAssetModalTagComponent<AssetT>>, Engine::Write<AssetPreviewDialogComponent>>;
  using Context = Engine::SystemContext<ImGuiView, Modifier, AssetView, ContextView, Engine::EntityFactory>;

  static std::shared_ptr<Engine::System> create() {
    return ecx::makeSystem("AssetInspector", &tickInspector, IMGUI_THREAD);
  }

  static void tickInspector(Context& context) {
    using namespace Engine;
    if(!context.get<ImGuiView>().tryGetFirst()) {
      return;
    }

    std::vector<Entity> toRemove;
    for(auto&& dialog : context.get<ContextView>()) {
      auto& modal = dialog.get<InspectedAssetModalComponent>();

      if(modal.mNeedsInit) {
        ImGui::OpenPopup(modal.mModalName.c_str());
        modal.mNeedsInit = false;
      }

      if(ImGui::Begin(modal.mModalName.c_str())) {
        auto endPopup = finally([]() { ImGui::End(); });
        //Button to close modal
        if(ImGui::Button("Close")) {
          ImGui::CloseCurrentPopup();
          //TODO: Clear preview
          toRemove.push_back(dialog.entity());
          break;
        }
        //Scroll view for asset list
        ImGui::BeginChild("assets", ImVec2(400, 200));
        auto endAssets = finally([]() { ImGui::EndChild(); });

        for(auto&& asset : context.get<AssetView>()) {
          const auto& assetInfo = asset.get<const AssetInfoComponent>();
          bool thisSelected = asset.entity() == modal.mCurrentSelection;
          bool wasSelected = thisSelected;
          ImGui::Selectable(assetInfo.mPath.cstr(), &thisSelected);
          //If selection changed this frame, set this as the selected asset
          if(thisSelected != wasSelected) {
            modal.mCurrentSelection = asset.entity();
            dialog.get<AssetPreviewDialogComponent>().mAsset = asset.entity();
          }

          if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            modal.mConfirmedSelection = asset.entity();
            break;
          }
        }
      }
    }

    auto factory = context.get<EntityFactory>();
    for(auto&& entity : toRemove) {
      factory.destroyEntity(entity);
    }
  }
};