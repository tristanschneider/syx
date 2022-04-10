#include "Precompile.h"
#include "Picker.h"

#include "ecs/component/EditorComponents.h"
#include "util/Finally.h"
#include <imgui/imgui.h>
#include "ImGuiImpl.h"
#include "util/ScratchPad.h"

namespace Picker {
  //TODO: get rid of this version, don't want scratch pad usage
  void createModal(const PickerInfo& info) {
    if(ImGui::BeginPopupModal(info.name, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      auto endPopup = finally([]() { ImGui::EndPopup(); });
      //Button to close modal
      if(ImGui::Button("Close")) {
        if(info.onCancel)
          info.onCancel();
        ImGui::CloseCurrentPopup();
        return;
      }
      //Scroll view for picker list
      ImGui::BeginChild("content", ImVec2(200, 200));
      auto endAssets = finally([]() { ImGui::EndChild(); });

      Variant* selected = IImGuiImpl::getPad().read(info.padKey);
      const size_t selectedId = selected ? std::get<size_t>(selected->mData) : 0;
      bool exited = false;
      info.forEachItem([selected, selectedId, &info, &exited](const char* itemName, size_t itemId, const void* item) {
        if(exited)
          return;

        const bool thisSelected = itemId == selectedId;
        bool wasSelected = thisSelected;
        ImGui::Selectable(itemName, &wasSelected);
        //If selection changed this frame, write the new selected item to the pad
        if(thisSelected != wasSelected) {
          const size_t newSelection = itemId;
          IImGuiImpl::getPad().write(info.padKey, Variant{ newSelection });
          if(info.onItemPreviewed)
            info.onItemPreviewed(item);
        }

        //Item confirmed
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          ImGui::CloseCurrentPopup();
          exited = true;
          if(info.onItemSelected)
            info.onItemSelected(item);
        }
      });
    }
  }

  ModalResult createModal(const ImmediatePickerInfo& info, PickerContextComponent& context) {
    if(ImGui::BeginPopupModal(info.name, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      auto endPopup = finally([]() { ImGui::EndPopup(); });
      //Button to close modal
      if(ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
        return ModalResult::Cancel;
      }
      //Scroll view for picker list
      ImGui::BeginChild("content", ImVec2(200, 200));
      auto endAssets = finally([]() { ImGui::EndChild(); });

      info.forEachItem([&context, &info](const char* itemName, size_t itemId) {
        const bool thisSelected = context.mSelectedID && itemId == *context.mSelectedID;
        bool wasSelected = thisSelected;
        ImGui::Selectable(itemName, &wasSelected);
        //If selection changed this frame, write the new selected item
        if(thisSelected != wasSelected) {
          context.mSelectedID = itemId;
          return PickItemResult::ItemPreviewed;
        }

        //Item confirmed
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          ImGui::CloseCurrentPopup();
          return PickItemResult::ItemSelected;
        }
        return PickItemResult::Continue;
      });
    }
    return ModalResult::Continue;
  }
}