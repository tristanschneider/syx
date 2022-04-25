#pragma once

#include "ecs/component/AssetComponent.h"
#include "ecs/component/EditorComponents.h"
#include "ecs/component/ImGuiContextComponent.h"
#include "ecs/system/editor/ObjectInspectorTraits.h"
#include "ecs/ECS.h"
#include "editor/Picker.h"
#include "imgui/imgui.h"
#include "TypeInfo.h"

struct ObjectInspectorSystemBase {
  static const char* WINDOW_NAME;
  static const char* COMPONENT_LIST;
  static const char* ADD_COMPONENT_BUTTON;
  static const char* REMOVE_COMPONENT_BUTTON;
  static const char* COMPONENT_PICKER_MODAL;

  static std::shared_ptr<Engine::System> init();
};

template<class T>
struct ObjectInspectorSystem {
};

template<class... Components>
struct ObjectInspectorSystem<ecx::TypeList<Components...>> {
  using ImGuiView = Engine::View<Engine::Include<ImGuiContextComponent>>;
  using Modifier = Engine::EntityModifier<Components...>;
  using SelectedView = Engine::View<Engine::Include<SelectedComponent>, Engine::OptionalWrite<Components>...>;
  using ContextView = Engine::View<Engine::Include<ObjectInspectorContextComponent>, Engine::Write<PickerContextComponent>>;
  using AssetView = Engine::View<Engine::Read<AssetInfoComponent>>;
  using ModalView = Engine::View<Engine::Read<InspectedAssetModalComponent>>;
  using AnyModalView = Engine::View<Engine::Include<ModalComponent>>;
  using Context = Engine::SystemContext<Engine::EntityFactory, ImGuiView, SelectedView, ContextView, Modifier, AssetView, ModalView, AnyModalView>;

  static std::shared_ptr<Engine::System> tick() {
    return ecx::makeSystem("InspectorTick", &_tick, IMGUI_THREAD);
  }

  static void _tick(Context& context) {
    using namespace Engine;
    if(!context.get<ImGuiView>().tryGetFirst()) {
      return;
    }

    //Always start window
    ImGui::Begin(ObjectInspectorSystemBase::WINDOW_NAME);

    //Manually sort to keep UI order consistent regardless of chunk moves
    std::vector<Entity> entities;
    SelectedView& selected = context.get<SelectedView>();
    for(auto&& entity : selected) {
      entities.push_back(entity.entity());
    }
    if(entities.empty()) {
      ImGui::End();
      return;
    }
    std::sort(entities.begin(), entities.end(), [](const Entity& l, const Entity& r) {
      return l.mData.mParts.mEntityId < r.mData.mParts.mEntityId;
    });

    //Only start components list section if something is selected
    ImGui::BeginChild(ObjectInspectorSystemBase::COMPONENT_LIST, ImVec2(0, 0), true);

    _showComponentPicker(context, entities);

    //Populate inspector for each entity
    for(const Entity& entity : entities) {
      ImGui::PushID(static_cast<int>(entity.mData.mParts.mEntityId));
      //Inspect all components for this entity
      int indexID = 0;
      (_inspectComponent<Components>(context, entity, indexID++), ...);
      //End of this entity
      ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::End();
  }

  static void _showComponentPicker(Context& context, const std::vector<Engine::Entity>& selectedEntities) {
    auto pickerContext = context.get<ContextView>().tryGetFirst();
    if(!pickerContext) {
      return;
    }

    Picker::ImmediatePickerInfo picker;
    picker.name = ObjectInspectorSystemBase::COMPONENT_PICKER_MODAL;

    //Present pick options for all component types
    picker.forEachItem = [&context, &selectedEntities](const Picker::ImmediatePickerInfo::ForEachCallback& callback) {
      std::optional<size_t> index = 0;
      (_tryPickItem<Components>(context, callback, index, selectedEntities), ...);
    };

    if(ImGui::Button(ObjectInspectorSystemBase::ADD_COMPONENT_BUTTON)) {
      ImGui::OpenPopup(picker.name);
    }

    Picker::createModal(picker, pickerContext->get<PickerContextComponent>());
  }

  template<class ComponentT>
  static void _tryPickItem(Context& context, const Picker::ImmediatePickerInfo::ForEachCallback& presentPick, std::optional<size_t>& index, const std::vector<Engine::Entity>& selectedEntities) {
    //Track context across all the _tryPickItem calls through this optional
    if(!index) {
      return;
    }
    else {
      ++(*index);
    }

    //Show this pick option only if none of the selected objects have it
    auto& view = context.get<SelectedView>();
    if(std::any_of(selectedEntities.begin(), selectedEntities.end(), [&view](auto&& entity) {
      auto it = view.find(entity);
      return it != view.end() && (*it).tryGet<ComponentT>() != nullptr;
    })) {
      return;
    }

    switch(presentPick(ecx::StaticTypeInfo<ComponentT>::getTypeName().c_str(), *index)) {
      //No preview for component picker
      case Picker::PickItemResult::ItemPreviewed:
      case Picker::PickItemResult::Continue:
        break;
      case Picker::PickItemResult::ItemSelected: {
        Modifier modifier = context.get<Modifier>();
        for(auto&& entity : selectedEntities) {
          modifier.addComponent<ComponentT>(entity);
        }
        //Stop iteration in the picker
        index.reset();
      }
    }
  }

  //Inspect a component for this entity
  template<class ComponentT>
  static void _inspectComponent(Context& context, const Engine::Entity& entity, int indexID) {
    auto& selectedView = context.get<SelectedView>();
    auto foundEntity = selectedView.find(entity);
    ComponentT* value = foundEntity != selectedView.end() ? (*foundEntity).tryGet<ComponentT>() : nullptr;
    if(!value) {
      return;
    }
    using TypeInfoT = ecx::StaticTypeInfo<ComponentT>;
    ImGui::PushID(indexID);
    ImGui::Text(TypeInfoT::getTypeName().c_str());
    ImGui::BeginGroup();

    //Visit all properties on this component and inspect them
    TypeInfoT::visitShallow([&](const std::string& memberName, auto& propValue) {
      using PropT = std::decay_t<decltype(propValue)>;
      using TraitsT = ObjectInspectorTraits<ComponentT, PropT>;

      if constexpr(IsModalInspectorT<TraitsT>::value) {
        _inspectModalProperty(typename TraitsT::ModalTy{}, memberName, entity, propValue, context);
      }
      else {
        ObjectInspectorTraits<ComponentT, PropT>::inspect(memberName.c_str(), propValue);
      }
    }, *value);

    if(ImGui::Button(ObjectInspectorSystemBase::REMOVE_COMPONENT_BUTTON)) {
      context.get<Modifier>().removeComponent<ComponentT>(entity);
    }

    //End of this component's properties
    ImGui::EndGroup();
    ImGui::Separator();
    ImGui::PopID();
  }

  template<class AssetT>
  static void _inspectModalProperty(AssetInspectorModal<AssetT>, const std::string& memberName, Engine::Entity self, Engine::Entity& memberValue, Context& context) {
    using namespace Engine;
    EntityFactory factory = context.get<EntityFactory>();

    AssetView& assets = context.get<AssetView>();

    FilePath valueName;
    if(auto foundAsset = assets.find(memberValue); foundAsset != assets.end()) {
      valueName = (*foundAsset).get<const AssetInfoComponent>().mPath;
    }
    else {
      valueName = "...";
    }

    //Left side label for property
    ImGui::Text(memberName.c_str());

    ImGui::SameLine();
    //Open selection modal on button press if a modal isn't already open
    if(ImGui::Button(valueName.cstr()) && !context.get<AnyModalView>().tryGetFirst()) {
      //Create the entity that the AssetInspectorSystem will populate when viewed
      auto&& [ modalEntity, modal, assetPreview, modalTag ] = factory.createAndGetEntityWithComponents<InspectedAssetModalComponent, AssetPreviewDialogComponent, ModalComponent>();
      modal.get().mCurrentSelection = memberValue;
      modal.get().mInspectedEntity = self;
      modal.get().mModalName = memberName;
      assetPreview.get().mAsset = memberValue;
    }
    ImGui::NewLine();

    //Check to see if the value has been set
    //Would be a bit more efficient to have selection on its own component. Presumably the number of dialogs is small
    for(auto&& dialog : context.get<ModalView>()) {
      //TODO: could clear these when selection is cleared
      const InspectedAssetModalComponent& modal = dialog.get<const InspectedAssetModalComponent>();
      if(modal.mInspectedEntity == self) {
        if(modal.mConfirmedSelection != Entity{}) {
          memberValue = modal.mConfirmedSelection;
          factory.destroyEntity(dialog.entity());
        }
        break;
      }
    }
  }
};
