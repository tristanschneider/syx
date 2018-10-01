#include "Precompile.h"
#include "ObjectInspector.h"

#include "editor/DefaultInspectors.h"
#include "editor/event/EditorEvents.h"
#include "event/BaseComponentEvents.h"
#include <event/EventBuffer.h>
#include <event/EventHandler.h>
#include "imgui/imgui.h"
#include "ImGuiImpl.h"
#include "lua/LuaVariant.h"
#include "LuaGameObject.h"
#include "provider/LuaGameObjectProvider.h"
#include "provider/MessageQueueProvider.h"

namespace {
  Component** _findComponent(std::vector<Component*>& components, size_t type) {
    for(Component*& c : components)
      if(c->getType() == type)
        return &c;
    return nullptr;
  }

  void _sortComponents(std::vector<Component*>& components) {
    // These first then whatever
    std::swap(components[0], *_findComponent(components, Component::typeId<NameComponent>()));
    std::swap(components[1], *_findComponent(components, Component::typeId<Transform>()));
    std::swap(components[2], *_findComponent(components, Component::typeId<SpaceComponent>()));
  }
}

ObjectInspector::ObjectInspector(MessageQueueProvider& msg, EventHandler& handler)
  : mMsg(msg)
  , mDefaultInspectors(std::make_unique<DefaultInspectors>()) {

  handler.registerEventHandler<SetSelectionEvent>([this](const SetSelectionEvent& e) {
    mSelected = e.mObjects;
  });
}

ObjectInspector::~ObjectInspector() {
}

void ObjectInspector::editorUpdate(const LuaGameObjectProvider& objects) {
  if(!ImGuiImpl::enabled()) {
    return;
  }

  if(mSelected != mPrevSelected)
    _updateSelection(objects);

  ImGui::Begin("Inspector");
  ImGui::BeginChild("ScrollView", ImVec2(0, 0), true);
  const size_t textLimit = 100;
  for(size_t i = 0; i < mSelected.size(); ++i) {
    auto& selected = mSelectedData[i];
    const LuaGameObject& original = *objects.getObject(mSelected[i]);

    ImGui::PushID(static_cast<int>(selected->getHandle()));

    std::vector<Component*> components(selected->componentCount());
    components.clear();
    selected->forEachComponent([&components](Component& c) {
      components.push_back(&c);
    });
    _sortComponents(components);

    for(size_t c = 0; c < components.size(); ++c) {
      Component* comp = components[c];

      bool updateComponent = false;
      if(const Lua::Node* props = comp->getLuaProps()) {
        ImGui::PushID(static_cast<int>(c));
        ImGui::Text(comp->getTypeInfo().mTypeName.c_str());
        ImGui::BeginGroup();

        //Populate editor for each property
        props->forEachDiff(~0, comp, [&updateComponent, this](const Lua::Node& prop, const void* data) {
          updateComponent = _inspectProperty(prop, const_cast<void*>(data)) || updateComponent;
        });

        //If a property changed, make a diff and send an update message
        if(updateComponent) {
          const auto& originalComp = original.getComponent(comp->getType(), comp->getSubType());
          const auto& newComp = *comp;

          //Generate diff of component
          auto diff = props->getDiff(&originalComp, &newComp);

          //Copy new values to buffer and send diff so only new value is updated
          std::vector<uint8_t> buffer(props->size());
          props->copyConstructToBuffer(&newComp, buffer.data());
          mMsg.getMessageQueue().get().push(SetComponentPropsEvent(newComp.getOwner(), newComp.getType(), props, diff, std::move(buffer)));
        }

        ImGui::EndGroup();
        ImGui::Separator();
        ImGui::PopID();
      }
    }

    ImGui::PopID();
  }

  ImGui::EndChild();
  ImGui::End();
}

void ObjectInspector::_updateSelection(const LuaGameObjectProvider& objects) {
  using namespace Lua;

  mPrevSelected = mSelected;
  mSelectedData.clear();
  for(Handle h : mSelected) {
    if(const LuaGameObject* obj = objects.getObject(h)) {
      mSelectedData.emplace_back(obj->clone());
    }
  }
}

bool ObjectInspector::_inspectProperty(const Lua::Node& prop, void* data) const {
  if(auto func = mDefaultInspectors->getFactory(prop))
    return (*func)(prop, data);
  //TODO: handle inspector overrides on prop
  return false;
}
