#include "Precompile.h"
#include "editor/SceneBrowser.h"

#include "editor/event/EditorEvents.h"
#include "event/BaseComponentEvents.h"
#include "event/eventBuffer.h"
#include "imgui/imgui.h"
#include "ImGuiImpl.h"
#include "LuaGameObject.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/MessageQueueProvider.h"

SceneBrowser::SceneBrowser(MessageQueueProvider* msg, GameObjectHandleProvider* handleGen)
  : mMsg(msg)
  , mHandleGen(handleGen) {
}

void SceneBrowser::editorUpdate(const HandleMap<std::unique_ptr<LuaGameObject>>& objects) {
  if(!ImGuiImpl::enabled()) {
    return;
  }
  ImGui::Begin("Objects");

  if(ImGui::Button("New Object")) {
    mSelected = mHandleGen->newHandle();
    mMsg->getMessageQueue().get().push(AddGameObjectEvent(mSelected));
  }

  if(ImGui::Button("Delete Object")) {
    const auto& it = objects.find(mSelected);
    if(it != objects.end()) {
      it->second->remove(mMsg->getMessageQueue().get());
    }
  }

  ImGui::BeginChild("ScrollView", ImVec2(0, 0), true);
  std::string name;
  for(const auto& it : objects) {
    const LuaGameObject& obj = *it.second;
    name = obj.getName().getName();
    //Make handle a hidden part of the id
    name += "##";
    name += std::to_string(obj.getHandle());
    if(ImGui::Selectable(name.c_str(), mSelected == obj.getHandle())) {
      mSelected = obj.getHandle();
      mMsg->getMessageQueue().get().push(PickObjectEvent(mSelected));
    }
  }
  ImGui::EndChild();

  ImGui::End();
}