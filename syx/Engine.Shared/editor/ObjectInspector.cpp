#include "Precompile.h"
#include "ObjectInspector.h"

#include "component/LuaComponentRegistry.h"
#include "editor/DefaultInspectors.h"
#include "editor/event/EditorEvents.h"
#include "editor/Picker.h"
#include "event/BaseComponentEvents.h"
#include <event/EventBuffer.h>
#include <event/EventHandler.h>
#include "imgui/imgui.h"
#include "ImGuiImpl.h"
#include "lua/LuaVariant.h"
#include "LuaGameObject.h"
#include "provider/ComponentRegistryProvider.h"
#include "provider/LuaGameObjectProvider.h"
#include "provider/MessageQueueProvider.h"
#include "util/Finally.h"
#include "util/ScratchPad.h"

namespace {
  Component** _findComponent(std::vector<Component*>& components, size_t type) {
    for(Component*& c : components)
      if(c->getType() == type)
        return &c;
    return nullptr;
  }

  void _sortComponents(std::vector<Component*>& components) {
    //These first then whatever
    std::swap(components[0], *_findComponent(components, Component::typeId<NameComponent>()));
    std::swap(components[1], *_findComponent(components, Component::typeId<Transform>()));
    std::swap(components[2], *_findComponent(components, Component::typeId<SpaceComponent>()));
  }
}

ObjectInspector::ObjectInspector(MessageQueueProvider& msg, EventHandler& handler, const ComponentRegistryProvider& componentRegistry)
  : mMsg(msg)
  , mDefaultInspectors(std::make_unique<DefaultInspectors>())
  , mComponentRegistry(componentRegistry) {

  handler.registerEventHandler([this](const SetSelectionEvent& e) {
    mSelected = e.mObjects;
  });

  auto refreshSelection = [this]() {
    mPrevSelected.clear();
  };
  handler.registerEventHandler([this, refreshSelection](const AddComponentEvent&) { refreshSelection(); });
  handler.registerEventHandler([this, refreshSelection](const RemoveComponentEvent&) { refreshSelection(); });
}

ObjectInspector::~ObjectInspector() {
}

void ObjectInspector::editorUpdate(const LuaGameObjectProvider& objects) {
  if(mSelected != mPrevSelected) {
    _updateSelection(objects);
  }

  ImGui::Begin("Inspector");
  ImGui::BeginChild("ScrollView", ImVec2(0, 0), true);

  _showComponentPicker();

  for(size_t i = 0; i < mSelected.size(); ++i) {
    auto& selected = mSelectedData[i];
    const LuaGameObject* original = objects.getObject(mSelected[i]);
    if(!original) {
      mSelected.clear();
      break;
    }

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
        props->forEachDiff(static_cast<Lua::NodeDiff>(~0), comp, [&updateComponent, this](const Lua::Node& prop, const void* data) {
          updateComponent = _inspectProperty(prop, const_cast<void*>(data)) || updateComponent;
        });

        //If a property changed, make a diff and send an update message
        if(updateComponent) {
          const Component& originalComp = *original->getComponent(comp->getType(), comp->getSubType());
          const Component& newComp = *comp;

          //Generate diff of component
          auto diff = props->getDiff(&originalComp, &newComp);

          //Copy new values to buffer and send diff so only new value is updated
          std::vector<uint8_t> buffer(props->size());
          props->copyConstructToBuffer(&newComp, buffer.data());
          mMsg.getMessageQueue().get().push(SetComponentPropsEvent(newComp.getOwner(), newComp.getFullType(), props, diff, std::move(buffer)));
        }

        if(!original->_isBuiltInComponent(*comp)) {
          _deleteComponentButton(*comp);
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

bool ObjectInspector::isSelected(Handle handle) const {
  return std::find(mSelected.begin(), mSelected.end(), handle) != mSelected.end();
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
  //Try an override first
  if(prop.hasInspector())
    return prop.inspect(data);
  //No override, use default inspector
  if(auto func = mDefaultInspectors->getFactory(prop))
    return (*func)(prop.getName().c_str(), data);
  return false;
}

void ObjectInspector::_showComponentPicker() const {
  Picker::PickerInfo picker;
  picker.name = "Components";
  picker.padKey = "selectedComponent";

  ScratchPad& pad = IImGuiImpl::getPad();
  pad.push("componentPicker");
  auto popPad = finally([&pad]() { pad.pop(); });

  picker.forEachItem = [this](const Picker::PickerInfo::ForEachCallback& callback) {
    mComponentRegistry.getReader().first.forEachComponent([&callback, this](const Component& type) {
      const bool canAddComponent = std::any_of(mSelectedData.begin(), mSelectedData.end(), [&type](const std::unique_ptr<LuaGameObject>& obj) {
        return obj->getComponent(type.getType(), type.getSubType()) == nullptr;
      });
      if(canAddComponent)
        callback(type.getTypeInfo().mTypeName.c_str(), type.getTypeInfo().mPropNameConstHash, &type);
    });
  };

  //Add the component to all selected items
  picker.onItemSelected = [this](const void* item) {
    const Component* type = reinterpret_cast<const Component*>(item);
    auto msg = mMsg.getMessageQueue();
    for(const auto& obj : mSelectedData) {
      if(!obj->getComponent(type->getType(), type->getSubType()))
        msg.get().push(AddComponentEvent(obj->getHandle(), type->getType(), type->getSubType()));
    }
  };

  if(ImGui::Button("Add Component")) {
    ImGui::OpenPopup(picker.name);
  }

  Picker::createModal(picker);
}

void ObjectInspector::_deleteComponentButton(const Component& component) {
  if(ImGui::Button("Remove Component")) {
    mMsg.getMessageQueue().get().push(RemoveComponentEvent(component.getOwner(), component.getType(), component.getSubType()));
  }
}
