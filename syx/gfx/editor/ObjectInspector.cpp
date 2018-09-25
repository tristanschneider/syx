#include "Precompile.h"
#include "ObjectInspector.h"

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

  bool _createEditor(const Lua::Node& prop, std::string& str) {
    const size_t textLimit = 100;
    str.reserve(textLimit);
    if(ImGui::InputText(prop.getName().c_str(), str.data(), textLimit)) {
      //Since we manually modified the internals of string, manually update size
      str.resize(std::strlen(str.data()));
      return true;
    }
    return false;
  }

  bool _createEditor(const Lua::Node& prop, bool& str) {
    return ImGui::Checkbox(prop.getName().c_str(), &str);
  }

  bool _createEditor(const Lua::Node& prop, void* data) {
    //TODO: register defaults for plain data somewhere, and allow overrides on the nodes
    if(prop.getTypeId() == typeId<std::string>())
      return _createEditor(prop, *reinterpret_cast<std::string*>(data));
    else if(prop.getTypeId() == typeId<bool>())
      return _createEditor(prop, *reinterpret_cast<bool*>(data));
    return false;
  }
}

ObjectInspector::ObjectInspector(MessageQueueProvider& msg, EventHandler& handler)
  : mMsg(msg) {

  handler.registerEventHandler<SetSelectionEvent>([this](const SetSelectionEvent& e) {
    mSelected = e.mObjects;
  });
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

    for(Component* comp : components) {
      bool updateComponent = false;
      if(const Lua::Node* props = comp->getLuaProps()) {
        ImGui::Text(comp->getTypeInfo().mTypeName.c_str());
        ImGui::BeginGroup();

        //Populate editor for each property
        props->forEachDiff(~0, comp, [&updateComponent](const Lua::Node& prop, const void* data) {
          updateComponent = _createEditor(prop, const_cast<void*>(data)) || updateComponent;
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