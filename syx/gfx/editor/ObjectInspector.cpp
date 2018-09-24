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
    ImGui::PushID(static_cast<int>(selected->getHandle()));
    //TODO: get a mutable reference through less nefarious means
    NameComponent& name = const_cast<NameComponent&>(selected->getName());
    //TODO: do similar approach but for all components, using known type ids to populate appropriate ui
    //TODO: detect when property has changed so property change message can be sent
    bool updateComponent = false;
    const Lua::Node* props = name.getLuaProps();
    props->forEachChildShallow([&name, textLimit, this, &updateComponent](const Lua::Node& prop) {
      if(prop.getTypeId() == typeId<std::string>()) {
        std::string* nameValue = reinterpret_cast<std::string*>(prop._translateBaseToNode(&name));
        nameValue->reserve(textLimit);
        if(ImGui::InputText(prop.getName().c_str(), nameValue->data(), textLimit)) {
          updateComponent = true;
          //Since we manually modified the internals of string, manually update size
          nameValue->resize(std::strlen(nameValue->data()));
        }
      }
    });

    //TODO: generalize this beyond name component
    if(updateComponent) {
      const LuaGameObject& original = *objects.getObject(mSelected[i]);
      const auto& originalComp = original.getName();
      const auto& newComp = selected->getName();

      //Generate diff of component
      auto diff = props->getDiff(&originalComp, &newComp);

      //Copy new values to buffer and send diff so only new value is updated
      std::vector<uint8_t> buffer(props->size());
      props->copyConstructToBuffer(&newComp, buffer.data());
      mMsg.getMessageQueue().get().push(SetComponentPropsEvent(newComp.getOwner(), newComp.getType(), props, diff, std::move(buffer)));
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