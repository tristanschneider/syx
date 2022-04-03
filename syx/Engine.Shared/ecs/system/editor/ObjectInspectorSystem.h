#pragma once

#include "ecs/component/EditorComponents.h"
#include "ecs/component/ImGuiContextComponent.h"
#include "ecs/system/editor/ObjectInspectorTraits.h"
#include "ecs/ECS.h"
#include "imgui/imgui.h"
#include "TypeInfo.h"

struct ObjectInspectorSystemBase {
  static const char* WINDOW_NAME;
  static const char* COMPONENT_LIST;
  static const char* ADD_COMPONENT_BUTTON;
  static const char* REMOVE_COMPONENT_BUTTON;
  static const char* COMPONENT_PICKER_MODAL;
};

template<class T>
struct ObjectInspectorSystem {
};

template<class... Components>
struct ObjectInspectorSystem<ecx::TypeList<Components...>> {
  using ImGuiView = Engine::View<Engine::Include<ImGuiContextComponent>>;
  using SelectedView = Engine::View<Engine::Include<SelectedComponent>, Engine::OptionalWrite<Components>...>;

  static std::shared_ptr<Engine::System> tick() {
    return ecx::makeSystem("InspectorTick", &_tick, IMGUI_THREAD);
  }

  static void _tick(Engine::SystemContext<ImGuiView, SelectedView>& context) {
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

    //Populate inspector for each entity
    for(const Entity& entity : entities) {
      ImGui::PushID(static_cast<int>(entity.mData.mParts.mEntityId));
      //Inspect all components for this entity
      if(auto it = selected.find(entity); it != selected.end()) {
        auto viewedEntity = *it;
        int indexID = 0;
        (_inspectComponent<Components>(viewedEntity, indexID++), ...);
      }
      //End of this entity
      ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::End();
  }

  //Inspect a component for this entity
  template<class ComponentT, class ViewedEntityT>
  static void _inspectComponent(ViewedEntityT& entity, int indexID) {
    ComponentT* value = entity.tryGet<ComponentT>();
    if(!value) {
      return;
    }
    using TypeInfoT = ecx::StaticTypeInfo<ComponentT>;
    ImGui::PushID(indexID);
    ImGui::Text(TypeInfoT::getTypeName().c_str());
    ImGui::BeginGroup();

    //Visit all properties on this component and inspect them
    TypeInfoT::visitShallow([](const std::string& memberName, auto& propValue) {
      using PropT = std::decay_t<decltype(propValue)>;
      ObjectInspectorTraits<ComponentT, PropT>::inspect(memberName.c_str(), propValue);
    }, *value);

    //TODO: delete component button

    //End of this component's properties
    ImGui::EndGroup();
    ImGui::Separator();
    ImGui::PopID();
  }
};